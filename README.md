# opencv_ffmpeg_streaming
rtmp streaming from opencv with ffmpeg / avcodec

Using ffmpeg libraries from C/C++ is tricky and I have found it easy to make mistakes or memory leaks.
I think that it might be helpful for others to have an example to start from that has no leaks or crashes.
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

A big thank you to the ffmpeg people for the amazing libs!!!

