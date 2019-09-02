project(simple_opencv_streaming)

cmake_minimum_required(VERSION 2.6.0 FATAL_ERROR)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set ( CMAKE_EXE_LINKER_FLAGS_RELEASE  "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -std=c++11 -Wall ")
set ( CMAKE_EXE_LINKER_FLAGS_DEBUG  "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined ")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}  -std=c++11 -Wall -O0 -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined -D__FORTIFY_SOURCE=2 -rdynamic -DDEBUG")

#set ( CMAKE_EXE_LINKER_FLAGS_DEBUG  "${CMAKE_EXE_LINKER_FLAGS_DEBUG}  ")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}  -std=c++11 -Wall -O0 -fno-omit-frame-pointer ")

find_package(OpenCV REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Test Concurrent)

#find_package(Qt5Core)

get_target_property(QtCore_location Qt5::Core /usr/lib/x86_64-linux-gnu/)


include(CMakeToolsHelpers OPTIONAL)

include_directories(${OpenCV_INCLUDE_DIRS})

add_subdirectory(streamer)

add_executable(simple_opencv_streaming main.cpp)


find_library(AVCODEC_LIBRARY avcodec)
find_library(AVFORMAT_LIBRARY avformat)
find_library(AVUTIL_LIBRARY avutil)
find_library(SWSCALE_LIBRARY swscale)

MESSAGE(${AVFORMAT_LIBRARY})


target_link_libraries(simple_opencv_streaming  streamer ${OpenCV_LIBS} ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} Qt5::Core)
#target_link_libraries(simple_opencv_streaming avcodec avformat avutil swscale avfilter avdevice)


