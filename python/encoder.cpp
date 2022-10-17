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
#include <pybind11/stl.h>

#include "ffmpeg_encoder.h"

namespace py = pybind11;
using namespace pybind11::literals;


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


class PythonEncoder : public Encoder
{
public:
    void put_frame(const py::array &frame, double frame_duration)
    {
        py::buffer_info info = frame.request();
        Encoder::put_frame(reinterpret_cast<const uint8_t*>(info.ptr), frame_duration);
    }
};


PYBIND11_MODULE(ffmpeg_encoder, m)
{
    m.doc() = "streaming from python";
    py::class_<EncoderConfig>(m, "EncoderConfig")
            .def(py::init<>())

            .def_readwrite("source_width", &EncoderConfig::src_width)
            .def_readwrite("source_height", &EncoderConfig::src_height)
            .def_readwrite("enc_width", &EncoderConfig::dst_width)
            .def_readwrite("enc_height", &EncoderConfig::dst_height)
            .def_readwrite("enc_fps", &EncoderConfig::fps)
            .def_readwrite("enc_bitrate", &EncoderConfig::bitrate)
            .def_readwrite("codec_params", &EncoderConfig::codec_params)
            .def_readwrite("codec_name", &EncoderConfig::codec_name)
            .def("set_mode_file", &EncoderConfig::set_mode_file)
            .def("set_mode_stream", &EncoderConfig::set_mode_stream)
            .def_readwrite("output", &EncoderConfig::output);


    py::class_<PythonEncoder>(m, "Encoder")
            .def(py::init<>())
            .def("init", &PythonEncoder::init)
            .def("enable_av_debug_log", &PythonEncoder::enable_av_debug_log)
            .def("close", &PythonEncoder::close)
            .def("put_frame", &PythonEncoder::put_frame);
}



