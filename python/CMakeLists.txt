cmake_minimum_required(VERSION 3.6.0 FATAL_ERROR)

project(rtmp_streaming VERSION 1.0.0 DESCRIPTION "rtmp streaming based on ffmpeg")


set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -std=c++14 -Wall")

set ( CMAKE_EXE_LINKER_FLAGS_DEBUG  "${CMAKE_EXE_LINKER_FLAGS_DEBUG}  ")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}  -std=c++14 -Wall -O0 -fno-omit-frame-pointer")


include_directories("../streamer")


find_package(Python 3.6 COMPONENTS Interpreter Development REQUIRED)

find_library(AVCODEC_LIBRARY avcodec)
find_library(AVFORMAT_LIBRARY avformat)
find_library(AVUTIL_LIBRARY avutil)
find_library(SWSCALE_LIBRARY swscale)
find_library(AVDEVICE_LIBRARY avdevice)


add_subdirectory(pybind11)
pybind11_add_module(rtmp_streaming rtmp_streaming.cpp ../streamer/streamer.cpp ../streamer/ffmpeg_encoder.cpp)
target_link_libraries(rtmp_streaming  PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${AVDEVICE_LIBRARY} ${SWSCALE_LIBRARY})


#pybind11_add_module(ffmpeg_encoder encoder.cpp ../streamer/ffmpeg_encoder.cpp)
#target_link_libraries(ffmpeg_encoder  PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${AVDEVICE_LIBRARY} ${SWSCALE_LIBRARY})
