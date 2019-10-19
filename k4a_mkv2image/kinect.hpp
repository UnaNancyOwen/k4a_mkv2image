#ifndef __KINECT__
#define __KINECT__

#include <k4a/k4a.hpp>
#include <k4arecord/playback.hpp>
#include <opencv2/opencv.hpp>

#if __has_include(<concurrent_queue.h>)
#include <concurrent_queue.h>
#else
#include <tbb/concurrent_queue.h>
namespace concurrency = tbb;
#endif

#include <thread>
#include <atomic>

#if __has_include(<filesystem>)
#include <filesystem>
namespace filesystem = std::filesystem;
#else
#include <experimental/filesystem>
#if _WIN32
namespace filesystem = std::experimental::filesystem::v1;
#else
namespace filesystem = std::experimental::filesystem;
#endif
#endif

#include "version.h"

class kinect
{
private:
    // Kinect
    k4a::playback playback;
    k4a::capture capture;
    k4a::calibration calibration;
    k4a::transformation transformation;
    k4a_record_configuration_t record_configuration;

    // Color
    k4a::image color_image;
    cv::Mat color;
    bool is_color;

    // Depth
    k4a::image depth_image;
    cv::Mat depth;
    bool is_depth;

    // Infrared
    k4a::image infrared_image;
    cv::Mat infrared;
    bool is_infrared;

    // Transformed
    k4a::image transformed_depth_image;
    cv::Mat transformed_depth;

    // Thread
    std::atomic_bool is_quit;
    std::thread color_thread;
    std::thread depth_thread;
    std::thread infrared_thread;
    concurrency::concurrent_queue<std::pair<std::vector<uint8_t>, int64_t>> color_queue;
    concurrency::concurrent_queue<std::pair<std::vector<uint16_t>, int64_t>> depth_queue;
    concurrency::concurrent_queue<std::pair<std::vector<uint16_t>, int64_t>> infrared_queue;

    // Option
    filesystem::path mkv_file;
    filesystem::path directory;
    std::vector<int32_t> params;
    bool is_scaling;
    bool is_transform;
    bool is_show;

public:
    // Constructor
    kinect( int argc, char* argv[] );

    // Destructor
    ~kinect();

    // Run
    void run();

    // Update
    void update();

    // Draw
    void draw();

    // Show
    void show();

private:
    // Initialize
    void initialize( int argc, char* argv[] );

    // Initialize Parameter
    void initialize_parameter( int argc, char* argv[] );

    // Initialize Playback
    void initialize_playback();

    // Initialize Save
    void initialize_save();

    // Finalize
    void finalize();

    // Export Color
    void export_color();

    // Export Depth
    void export_depth();

    // Export Infrared
    void export_infrared();

    // Update Frame
    void update_frame();

    // Update Color
    void update_color();

    // Update Depth
    void update_depth();

    // Update Infrared
    void update_infrared();

    // Update Transformation
    void update_transformation();

    // Draw Color
    void draw_color();

    // Draw Depth
    void draw_depth();

    // Draw Infrared
    void draw_infrared();

    // Draw Transformation
    void draw_transformation();

    // Show Color
    void show_color();

    // Show Depth
    void show_depth();

    // Show Infrared
    void show_infrared();

    // Show Transformation
    void show_transformation();
};

#endif // __KINECT__
