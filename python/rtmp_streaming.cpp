#include <boost/python.hpp>
#include <set>
#include <map>
#include <memory>
#include <iostream>
#include <chrono>
#include <stdlib.h>
#include <math.h>
#include <cstdio>
#include <boost/python/numpy.hpp>

#include "streamer.hpp"

namespace np = boost::python::numpy;

using namespace streamer;

extern "C"
{
void __attribute__ ((constructor)) lib_init(void);
void __attribute__ ((destructor)) lib_fini(void);
}


void __attribute__ ((constructor)) lib_init(void)
{
    np::initialize();
}

void __attribute__ ((destructor)) lib_fini(void)
{

}


static char const* version()
{
   return "1.0";
}


void print_shape(const char *array_name, const np::ndarray &array)
{
    printf("%s shape: [", array_name);
    int nd = array.get_nd();
    for(int i=0;i<nd;i++) {
        printf("%ld ", array.shape(i));
    }
    printf("]\n");
}


class PythonStreamer : public Streamer
{
public:
    void stream_frame(const np::ndarray &frame)
    {
        //print_shape("frame", frame);
        Streamer::stream_frame(reinterpret_cast<const uint8_t*>(frame.get_data()));
    }

    void stream_frame_with_duration(const np::ndarray &frame, int64_t frame_duration)
    {
        //print_shape("frame", frame);
        Streamer::stream_frame(reinterpret_cast<const uint8_t*>(frame.get_data()), frame_duration);
    }
};


BOOST_PYTHON_MODULE(rtmp_streaming)
{
    using namespace boost::python;
    def("version", version);

    class_<StreamerConfig>("StreamerConfig", init<>())
            .def(init<int, int, int, int, int, int,
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

    class_<PythonStreamer>("Streamer", init<>())
            .def("init", &PythonStreamer::init)
            .def("enable_av_debug_log", &PythonStreamer::enable_av_debug_log)
            .def("stream_frame", &PythonStreamer::stream_frame)
            .def("stream_frame_with_duration", &PythonStreamer::stream_frame_with_duration);
}






