k4a_mkv2image
=============
This is convert tool that works on cross-platform (Windows, Linux) for Azure Kinect.  
This tool converts all data of each stream types (Color, Depth, Infrared) that contained in mkv file to image file.  

Sample
------
### Command
```
k4a_mkv2image -i="./file.mkv" -s=true -d=true
```
### Output
Image files are output in the following folder structure.  
Image file name is "\<file_index\>_\<device_timestamp\>.jpg".  
The file index is sequencial number for output images. 
The unit of device timestamp is microseconds.  
```
k4a_mkv2image.exe
file.mkv
file
  |-color
  |   |-000000_00000000000.jpg
  |   |-000001_00000000033.jpg
  |   |-000002_00000000066.jpg
  |
  |-depth
  |   |-000000_00000000000.png
  |   |-000001_00000000033.png
  |   |-000002_00000000066.png
  |
  |-infrared
      |-000000_00000000000.jpg
      |-000001_00000000033.jpg
      |-000002_00000000066.jpg
```

Option
------
| option | description                                                                           |
|:------:|:--------------------------------------------------------------------------------------|
| -i     | input mkv file path. (requered)                                                       |
| -s     | enable depth scaling. <code>false</code> is raw 16bit image. (bool)                   |
| -t     | enable transform depth to color camera. <code>false</code> is not transform. (bool)   |
| -q     | jpeg encoding quality for infrared. [0-100]                                           |
| -d     | display each images on window. <code>false</code> is not display. (bool)              |

Environment
-----------
* Visual Studio 2017/2019 / GCC 7.4 / Clang 6.0 (require <code>\<filesystem\></code> supported) 
* Azure Kinect Sensor SDK v1.3.0 (or later)
* OpenCV 4.1.2 (or later)
* CMake 3.15.4 (latest release is preferred)
* Intel Thread Building Blocks (latest release is preferred)

<sup>&#042; This tool requires Intel TBB only on Linux.</sup>  

License
-------
Copyright &copy; 2019 Tsukasa SUGIURA  
Distributed under the [MIT License](http://www.opensource.org/licenses/mit-license.php "MIT License | Open Source Initiative").

Contact
-------
* Tsukasa Sugiura  
    * <t.sugiura0204@gmail.com>  
    * <http://unanancyowen.com>  