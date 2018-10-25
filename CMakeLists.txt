project(simple_opencv_streaming)

cmake_minimum_required(VERSION 2.6.0 FATAL_ERROR)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set ( CMAKE_EXE_LINKER_FLAGS_RELEASE  "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -fopenmp ")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -std=c++11 -Wall -fopenmp")
set ( CMAKE_EXE_LINKER_FLAGS_DEBUG  "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined -fopenmp")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}  -std=c++11 -Wall -O0 -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined -D__FORTIFY_SOURCE=2 -rdynamic -DDEBUG -fopenmp")

#set ( CMAKE_EXE_LINKER_FLAGS_DEBUG  "${CMAKE_EXE_LINKER_FLAGS_DEBUG}  -fopenmp")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}  -std=c++11 -Wall -O0 -fno-omit-frame-pointer -fopenmp")


find_package(OpenCV 3 REQUIRED)

include(CMakeToolsHelpers OPTIONAL)

include_directories(${OpenCV_INCLUDE_DIRS})
#include_directories(streamer)

add_subdirectory(streamer)


add_executable(simple_opencv_streaming main.cpp)

target_link_libraries(simple_opencv_streaming  streamer ${OpenCV_LIBS})



target_link_libraries(simple_opencv_streaming avcodec avformat avutil swscale avfilter avdevice)
