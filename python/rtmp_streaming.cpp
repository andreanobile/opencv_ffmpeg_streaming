#include <set>
#include <map>
#include <memory>
#include <iostream>
#include <chrono>
#include <stdlib.h>
#include <math.h>
#include <cstdio>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include "streamer.hpp"

namespace py = pybind11;
using namespace pybind11::literals;

using namespace streamer;

void print_shape(const char *array_name, const py::array &array)
{
    printf("%s shape: [", array_name);

    //py::buffer_info info = array.request();
    
    int nd = array.ndim();
    
    for(int i=0;i<nd;i++) {
        printf("%ld ", array.shape()[i]);
    }
    printf("]\n");
}


class PythonStreamer : public Streamer
{
public:
    void stream_frame(const py::array &frame)
    {
        //print_shape("frame", frame);
        py::buffer_info info = frame.request();
        Streamer::stream_frame(reinterpret_cast<const uint8_t*>(info.ptr));
    }

    void stream_frame_with_duration(const py::array &frame, int64_t frame_duration)
    {
        py::buffer_info info = frame.request();
        Streamer::stream_frame(reinterpret_cast<const uint8_t*>(info.ptr), frame_duration);
    }
};




PYBIND11_MODULE(rtmp_streaming, m)
{
    m.doc() = "streaming from python";
    py::class_<StreamerConfig>(m, "StreamerConfig")
            .def(py::init<>())
            .def(py::init<int, int, int, int, int, int,
                 const std::string &,
                 const std::string &>())
            .def_readwrite("source_width", &StreamerConfig::src_width)
            .def_readwrite("source_height", &StreamerConfig::src_height)
            .def_readwrite("stream_width", &StreamerConfig::dst_width)
            .def_readwrite("stream_height", &StreamerConfig::dst_height)
            .def_readwrite("stream_fps", &StreamerConfig::fps)
            .def_readwrite("stream_bitrate", &StreamerConfig::bitrate)
            .def_readwrite("stream_profile", &StreamerConfig::profile)
            .def_readwrite("stream_server", &StreamerConfig::server);

    py::class_<PythonStreamer>(m, "Streamer")
            .def(py::init<>())
            .def("init", &PythonStreamer::init)
            .def("enable_av_debug_log", &PythonStreamer::enable_av_debug_log)
            .def("stream_frame", &PythonStreamer::stream_frame)
            .def("stream_frame_with_duration", &PythonStreamer::stream_frame_with_duration);
}



