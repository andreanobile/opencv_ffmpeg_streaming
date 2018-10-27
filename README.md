# opencv_ffmpeg_streaming
rtmp streaming from opencv with ffmpeg / avcodec

Using ffmpeg libraries from C/C++ is tricky and I could not easily find easy examples without memory leaks or bad crashes that used OpenCV as input or for image processing.
This is an example for using the ffmpeg and opencv libraries from C++ for rtmp streaming. 
The input is from the OpenCV VideoCapture class.
To build Release:
```
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make
```

To build Debug:
```
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Debug ..
$ make
```
In main.cpp the stream url is defined as "rtmp://localhost/live/mystream" and must be adapted to your rtmp server settings

Run the program on a video:
```
$ ./build/simple_opencv_streaming <video_file>
```
From camera
```
$ ./build/simple_opencv_streaming 0
```

