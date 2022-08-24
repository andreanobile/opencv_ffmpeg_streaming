cmake_minimum_required(VERSION 3.6.0 FATAL_ERROR)

project(rtmp_streaming VERSION 1.0.0 DESCRIPTION "rtmp streaming based on ffmpeg")


set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -std=c++14 -Wall")

set ( CMAKE_EXE_LINKER_FLAGS_DEBUG  "${CMAKE_EXE_LINKER_FLAGS_DEBUG}  ")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}  -std=c++14 -Wall -O0 -fno-omit-frame-pointer")

#find_package(PythonInterp 3.6 REQUIRED)
#find_package(PythonLibs 3.6 REQUIRED)

#set(Python_ROOT_DIR "")


#set(BOOST_PYTHONLIB python3)

#find_package(Boost COMPONENTS ${BOOST_PYTHONLIB})

#include(CMakeToolsHelpers OPTIONAL)


#include_directories(${PYTHON_INCLUDE_DIRS})


#include_directories(${Boost_INCLUDE_DIR} ${PYTHON_INCLUDE} "../streamer")
#link_directories(${Boost_LIBRARY_DIR})

include_directories("../streamer")


find_package(Python 3.6 COMPONENTS Interpreter Development REQUIRED)

find_library(AVCODEC_LIBRARY avcodec)
find_library(AVFORMAT_LIBRARY avformat)
find_library(AVUTIL_LIBRARY avutil)
find_library(SWSCALE_LIBRARY swscale)

#add_library(rtmp_streaming SHARED
#    rtmp_streaming.cpp ../streamer/streamer.cpp
#)



#SET_TARGET_PROPERTIES(rtmp_streaming PROPERTIES PREFIX "")

#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

#target_link_libraries(rtmp_streaming  ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${Boost_LIBRARIES}  boost_numpy3 /usr/lib/x86_64-linux-gnu/libpython3.6m.so)

add_subdirectory(pybind11)
pybind11_add_module(rtmp_streaming rtmp_streaming.cpp ../streamer/streamer.cpp)
target_link_libraries(rtmp_streaming  PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY})
