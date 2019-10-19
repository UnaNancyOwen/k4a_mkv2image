#include "kinect.hpp"
#include "util.h"

#include <chrono>
#include <fstream>

// Constructor
kinect::kinect( int argc, char* argv[] )
    : is_quit( true ),
      is_color( false ),
      is_depth( false ),
      is_infrared( false )
{
    // Initialize
    initialize( argc, argv );
}

kinect::~kinect()
{
    // Finalize
    finalize();
}

// Initialize
void kinect::initialize( int argc, char* argv[] )
{
    // Initialize Parameter
    initialize_parameter( argc, argv );

    // Initialize Playback
    initialize_playback();

    // Initialize Save
    initialize_save();
}

// Initialize Parameter
void kinect::initialize_parameter( int argc, char* argv[] )
{
    // Create Command Line Parser
    const std::string keys =
        "{ help h      |       | print this message.                                                                                                          }"
        "{ input i     |       | path to input mkv file. (required)                                                                                           }"
        "{ scaling s   | false | enable depth and infrared scaling to 8bit image. false is raw 16bit image. (bool)                                            }"
        "{ transform t | false | enable transform depth image to color camera. (bool)                                                                         }"
        "{ quality q   | 95    | jpeg encoding quality for infrared. [0-100]                                                                                  }"
        "{ display d   | false | display each images on window. false is not display. display images are always scaled regardless of the scaling flag. (bool) }";
    cv::CommandLineParser parser( argc, argv, keys );

    if( parser.has( "help" ) ){
        std::cout << "k4a_mkv2image v" << K4A_MKV2IMAGE_VERSION << std::endl;
        parser.printMessage();
        std::exit( EXIT_SUCCESS );
    }

    // Check Parsing Error
    if( !parser.check() ){
        parser.printErrors();
        throw std::runtime_error( "failed command arguments" );
    }

    // Get MKV File Path (Required)
    if( !parser.has( "input" ) ){
        throw std::runtime_error( "failed can't find input mkv file" );
    }
    else{
        mkv_file = parser.get<cv::String>( "input" ).c_str();
        if( !filesystem::is_regular_file( mkv_file ) || mkv_file.extension() != ".mkv" ){
            throw std::runtime_error( "failed can't find input mkv file" );
        }
    }

    // Get Scaling Flag (Option)
    if( !parser.has( "scaling" ) ){
        is_scaling = false;
    }
    else{
        is_scaling = parser.get<bool>( "scaling" );
    }

    // Get Transformation Flag (Option)
    if( !parser.has( "transform" ) ){
        is_transform = false;
    }
    else{
        is_transform = parser.get<bool>( "transform" );
    }

    // Get JPEG Quality (Option)
    if( !parser.has( "quality" ) ){
        params = { cv::IMWRITE_JPEG_QUALITY, 95 };
    }
    else{
        params = { cv::IMWRITE_JPEG_QUALITY, std::min( std::max( 0, parser.get<int32_t>( "quality" ) ), 100 ) };
    }

    // Get Display Flag (Option)
    if( !parser.has( "display" ) ){
        is_show = false;
    }
    else{
        is_show = parser.get<bool>( "display" );
    }
}

// Initialize Playback
inline void kinect::initialize_playback()
{
    if( !filesystem::is_regular_file( mkv_file ) || !filesystem::exists( mkv_file ) ){
        throw k4a::error( "Failed to found file path!" );
    }

    // Open Playback
    playback = k4a::playback::open( mkv_file.generic_string().c_str() );

    // Get Record Configuration
    record_configuration = playback.get_record_configuration();

    // Get Calibration
    calibration = playback.get_calibration();

    // Create Transformation
    transformation = k4a::transformation( calibration );
}

// Initialize Save
void kinect::initialize_save()
{
    // Create Root Directory (MKV File Name)
    directory = mkv_file.parent_path().generic_string() + "/" + mkv_file.stem().string();
    if( !filesystem::create_directories( directory ) ){
        throw std::runtime_error( "failed can't create root directory" );
    }

    // Create Sub Directory for Each Images (Image Name)
    std::vector<std::string> names;
    if( record_configuration.color_track_enabled ){
        names.push_back( "color" );
        is_color = true;
    }
    if( record_configuration.depth_track_enabled ){
        names.push_back( "depth" );
        is_depth = true;
    }
    if( record_configuration.ir_track_enabled ){
        names.push_back( "infrared" );
        is_infrared = true;
    }

    for( const std::string& name : names ){
        filesystem::path sub_directory = directory.generic_string() + "/" + name;
        if( !filesystem::create_directories( sub_directory ) ){
            throw std::runtime_error( "failed can't create sub directory (" + name + ")" );
        }
    }

    // Start Threads
    if( is_color ){
        color_thread = std::thread( &kinect::export_color, this );
    }
    if( is_depth ){
        depth_thread = std::thread( &kinect::export_depth, this );
    }
    if( is_infrared ){
        infrared_thread = std::thread( &kinect::export_infrared, this );
    }
}

// Finalize
void kinect::finalize()
{
    // Destroy Transformation
    transformation.destroy();

    // Close Playback
    playback.close();

    // Join Threads
    is_quit = true;
    if( color_thread.joinable() ){
        color_thread.join();
    }
    if( depth_thread.joinable() ){
        depth_thread.join();
    }
    if( infrared_thread.joinable() ){
        infrared_thread.join();
    }

    // Close Window
    cv::destroyAllWindows();
}

// Export Color
void kinect::export_color()
{
    assert( record_configuration.color_format == k4a_image_format_t::K4A_IMAGE_FORMAT_COLOR_MJPG );

    is_quit = false;
    uint64_t index = 0;

    while( !( is_quit && color_queue.empty() ) ){
        // Pop Queue
        std::pair<std::vector<uint8_t>, int64_t> data;
        bool result = color_queue.try_pop( data );
        if( !result ){
            std::this_thread::yield();
            continue;
        }

        // Write JPEG (e.g. ./<mkv_name>/color/000000_<timestamp>.jpg, ...)
        std::ofstream ofs( cv::format( "%s/color/%06d_%011d.jpg", directory.generic_string().c_str(), index++, data.second ), std::ios::binary );
        ofs.write( reinterpret_cast<char*>( &data.first[0] ), data.first.size() * sizeof( decltype( data.first )::value_type ) );
    }
}

// Export Depth
void kinect::export_depth()
{
    is_quit = false;
    uint64_t index = 0;

    const int32_t width  = ( is_transform ) ? calibration.color_camera_calibration.resolution_width  : calibration.depth_camera_calibration.resolution_width;
    const int32_t height = ( is_transform ) ? calibration.color_camera_calibration.resolution_height : calibration.depth_camera_calibration.resolution_height;

    while( !( is_quit && depth_queue.empty() ) ){
        // Pop Queue
        std::pair<std::vector<uint16_t>, int64_t> data;
        bool result = depth_queue.try_pop( data );
        if( !result ){
            std::this_thread::yield();
            continue;
        }

        // Write PNG (e.g. ./<mkv_name>/depth/000000_<timestamp>.png, ...)
        cv::Mat depth = cv::Mat( height, width, CV_16UC1, reinterpret_cast<uint16_t*>( &data.first[0] ) );
        if( is_scaling ){
            depth.convertTo( depth, CV_8U, -255.0 / 5000.0, 255.0 );
        }
        cv::imwrite( cv::format( "%s/depth/%06d_%011d.png", directory.generic_string().c_str(), index++, data.second ), depth );
    }
}

// Export Infrared
void kinect::export_infrared()
{
    is_quit = false;
    uint64_t index = 0;

    const int32_t width  = calibration.depth_camera_calibration.resolution_width;
    const int32_t height = calibration.depth_camera_calibration.resolution_height;

    while( !( is_quit && infrared_queue.empty() ) ){
        // Pop Queue
        std::pair<std::vector<uint16_t>, int64_t> data;
        bool result = infrared_queue.try_pop( data );
        if( !result ){
            std::this_thread::yield();
            continue;
        }

        // Write JPEG (e.g. ./<mkv_name>/infrared/000000_<timestamp>.jpg, ...)
        cv::Mat infrared = cv::Mat( height, width, CV_16UC1, reinterpret_cast<uint16_t*>( &data.first[0] ) );
        infrared.convertTo( infrared, CV_8U, 0.5 );
        cv::imwrite( cv::format( "%s/infrared/%06d_%011d.jpg", directory.generic_string().c_str(), index++, data.second ), infrared, params );
    }
}

// Run
void kinect::run()
{
    // Main Loop
    while( true ){
        // Update
        update();

        // Draw
        draw();

        // Show
        show();

        // Wait Key
        constexpr int32_t delay = 1;
        const int32_t key = cv::waitKey( delay );
        if( key == 'q' ){
            break;
        }
    }
}

// Update
void kinect::update()
{
    // Update Frame
    update_frame();

    // Update Color
    update_color();

    // Update Depth
    update_depth();

    // Update Infrared
    update_infrared();

    // Update Transformation
    if( is_transform ){
        update_transformation();
    }

    // Release Capture Handle
    capture.reset();
}

// Update Frame
inline void kinect::update_frame()
{
    // Get Capture Frame
    const bool result = playback.get_next_capture( &capture );
    if( !result ){
        // EOF
        std::exit( EXIT_SUCCESS );
    }
}

// Update Color
inline void kinect::update_color()
{
    if( !is_color ){
        return;
    }

    // Get Color Image
    color_image = capture.get_color_image();

    if( !color_image.handle() ){
        return;
    }

    // Push Queue
    color_queue.push( std::make_pair( std::vector<uint8_t>( color_image.get_buffer(), color_image.get_buffer() + color_image.get_size() ), color_image.get_device_timestamp().count() ) );
}

// Update Depth
inline void kinect::update_depth()
{
    if( !is_depth ){
        return;
    }

    // Get Depth Image
    depth_image = capture.get_depth_image();

    if( !depth_image.handle() ){
        return;
    }

    if( is_transform ){
        return;
    }

    // Push Queue
    depth_queue.push( std::make_pair( std::vector<uint16_t>( reinterpret_cast<uint16_t*>( depth_image.get_buffer() ), reinterpret_cast<uint16_t*>( depth_image.get_buffer() + depth_image.get_size() ) ), depth_image.get_device_timestamp().count() ) );
}

// Update Infrared
void kinect::update_infrared()
{
    if( !is_infrared ){
        return;
    }

    // Get Infrared Image
    infrared_image = capture.get_ir_image();

    if( !infrared_image.handle() ){
        return;
    }

    // Push Queue
    infrared_queue.push( std::make_pair( std::vector<uint16_t>( reinterpret_cast<uint16_t*>( infrared_image.get_buffer() ), reinterpret_cast<uint16_t*>( infrared_image.get_buffer() + infrared_image.get_size() ) ), infrared_image.get_device_timestamp().count() ) );
}

// Update Transformation
inline void kinect::update_transformation()
{
    if( !depth_image.handle() ){
        return;
    }

    // Transform Depth Image to Color Camera
    transformed_depth_image = transformation.depth_image_to_color_camera( depth_image );

    if( !is_transform ){
        return;
    }

    depth_queue.push( std::make_pair( std::vector<uint16_t>( reinterpret_cast<uint16_t*>( transformed_depth_image.get_buffer() ), reinterpret_cast<uint16_t*>( transformed_depth_image.get_buffer() + transformed_depth_image.get_size() ) ), depth_image.get_device_timestamp().count() ) );
}

// Draw
void kinect::draw()
{
    // Draw Color
    draw_color();

    // Draw Depth
    draw_depth();

    // Draw Transformation
    draw_transformation();

    // Draw Infrared
    draw_infrared();
}

// Draw Color
inline void kinect::draw_color()
{
    if( !color_image.handle() ){
        return;
    }

    if( is_show ){
        // Get cv::Mat from k4a::image
        color = k4a::get_mat( color_image );
    }

    // Release Color Image Handle
    color_image.reset();
}

// Draw Depth
inline void kinect::draw_depth()
{
    if( !depth_image.handle() ){
        return;
    }

    if( is_show ){
        // Get cv::Mat from k4a::image
        depth = k4a::get_mat( depth_image );
    }

    // Release Depth Image Handle
    depth_image.reset();
}

// Draw Infrared
void kinect::draw_infrared()
{
    if( !infrared_image.handle() ){
        return;
    }

    if( is_show ){
        // Get cv::Mat from k4a::image
        infrared = k4a::get_mat( infrared_image );
    }

    // Release Infrared Image Handle
    infrared_image.reset();
}

// Draw Transformation
inline void kinect::draw_transformation()
{
    if( !transformed_depth_image.handle() ){
        return;
    }

    if( is_show ){
        // Get cv::Mat from k4a::image
        transformed_depth = k4a::get_mat( transformed_depth_image );
    }

    // Release Transformed Image Handle
    transformed_depth_image.reset();
}

// Show
void kinect::show()
{
    if( !is_show ){
        return;
    }

    // Show Color
    show_color();

    if( !is_transform ){
        // Show Depth
        show_depth();
    }
    else{
        // Show Transformation
        show_transformation();
    }

    // Show Infrared
    show_infrared();
}

// Show Color
inline void kinect::show_color()
{
    if( color.empty() ){
        return;
    }

    // Show Image
    const cv::String window_name = cv::format( "color" );
    cv::imshow( window_name, color );
}

// Show Depth
inline void kinect::show_depth()
{
    if( depth.empty() ){
        return;
    }

    // Scaling Depth
    depth.convertTo( depth, CV_8U, -255.0 / 5000.0, 255.0 );

    // Show Image
    const cv::String window_name = cv::format( "depth" );
    cv::imshow( window_name, depth );
}

// Show Infrared
void kinect::show_infrared()
{
    if( infrared.empty() ){
        return;
    }

    // Scaling Infrared
    infrared.convertTo( infrared, CV_8U, 0.5 );

    // Show Image
    const cv::String window_name = cv::format( "infrared" );
    cv::imshow( window_name, infrared );
}

// Show Transformation
inline void kinect::show_transformation()
{
    if( transformed_depth.empty() ){
        return;
    }

    // Scaling Depth
    transformed_depth.convertTo( transformed_depth, CV_8U, -255.0 / 5000.0, 255.0 );

    // Show Image
    const cv::String window_name = cv::format( "transformed depth" );
    cv::imshow( window_name, transformed_depth );
}
