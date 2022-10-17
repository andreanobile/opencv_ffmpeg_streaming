#include <set>
#include <map>
#include <memory>
#include <iostream>
#include <chrono>
#include <stdlib.h>
#include <math.h>
#include <cstdio>
#include <thread>
#include <deque>
#include <memory>
#include <mutex>
#include <atomic>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include "streamer.hpp"

#include "ffmpeg_encoder.h"

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


class PythonEncoder : public Encoder
{
public:
    void put_frame(const py::array &frame, double frame_duration)
    {
        py::buffer_info info = frame.request();
        Encoder::put_frame(reinterpret_cast<const uint8_t*>(info.ptr), frame_duration);
    }

    void write(const py::array &frame)
    {
        py::buffer_info info = frame.request();
        Encoder::put_frame(reinterpret_cast<const uint8_t*>(info.ptr), 0.0);
    }
};

enum ENCODER_COMMAND {
    ENC_INIT,
    ENC_FRAME,
    ENC_CLOSE
};

struct EncoderQueueItem
{
    ENCODER_COMMAND cmd;
    std::vector<uint8_t> frame;
    std::unique_ptr<EncoderConfig> config;
    double frame_duration = 0.0;
};

class PythonEncoderAsync;
void cmd_thread(PythonEncoderAsync *encoder);

class PythonEncoderAsync : public Encoder
{
public:

    PythonEncoderAsync()
    {
        th = std::thread(cmd_thread, this);
    }

//    void put_frame(const py::array &frame, double frame_duration)
//    {
//        py::buffer_info info = frame.request();
//        Encoder::put_frame(reinterpret_cast<const uint8_t*>(info.ptr), frame_duration);
//    }

    void write(const py::array &frame)
    {
        py::buffer_info info = frame.request();
        auto cmd = std::make_shared<EncoderQueueItem>();
        cmd->cmd = ENC_FRAME;
        cmd->frame_duration = 0.0;
        cmd->frame.resize(info.size * sizeof(uint8_t));
        memcpy(cmd->frame.data(), reinterpret_cast<const uint8_t*>(info.ptr), info.size * sizeof(uint8_t));
        while(!queue_push(cmd)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    void init(const EncoderConfig &config)
    {
        auto cmd = std::make_shared<EncoderQueueItem>();
        cmd->cmd = ENC_INIT;
        cmd->config = std::make_unique<EncoderConfig>();
        *(cmd->config) = config;
        while(!queue_push(cmd)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    void close()
    {
        auto cmd = std::make_shared<EncoderQueueItem>();
        cmd->cmd = ENC_CLOSE;
        while(!queue_push(cmd)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    // TODO: implement backpressure
    bool queue_push(std::shared_ptr<EncoderQueueItem> &cmd)
    {
        const std::lock_guard<std::mutex> lock(cmd_mutex);
        cmd_queue.push_back(cmd);
        return true;
    }

    bool queue_pop(std::shared_ptr<EncoderQueueItem> *cmd)
    {
        const std::lock_guard<std::mutex> lock(cmd_mutex);
        if (cmd_queue.size()) {
            *cmd = cmd_queue.front();
            cmd_queue.pop_front();
            return true;
        }
        return false;
    }

    bool execute(std::shared_ptr<EncoderQueueItem> &cmd)
    {
        if (cmd->cmd == ENC_INIT) {
            Encoder::init(*(cmd->config));
        } else if (cmd->cmd == ENC_CLOSE) {
            Encoder::close();
        } else if(cmd->cmd == ENC_FRAME) {
            Encoder::put_frame(cmd->frame.data(), cmd->frame_duration);
        }
        return true;
    }

    ~PythonEncoderAsync()
    {
        running = false;
        if (th.joinable()) {
            th.join();
        }
    }

    std::deque<std::shared_ptr<EncoderQueueItem>> cmd_queue;
    std::mutex cmd_mutex;
    std::thread th;
    std::atomic<bool> running{true};
};

void cmd_thread(PythonEncoderAsync *encoder)
{
    while(encoder->running) {
        std::shared_ptr<EncoderQueueItem> cmd;
        bool has_command = encoder->queue_pop(&cmd);
        if (has_command) {
            encoder->execute(cmd);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

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
            .def_readwrite("output", &EncoderConfig::output)
            .def("set_mode_file", &EncoderConfig::set_mode_file)
            .def("set_mode_stream", &EncoderConfig::set_mode_stream);

    py::class_<PythonEncoder>(m, "Encoder")
            .def(py::init<>())
            .def("init", &PythonEncoder::init)
            .def("enable_av_debug_log", &PythonEncoder::enable_av_debug_log)
            .def("close", &PythonEncoder::close)
            .def("release", &PythonEncoder::close)
            .def("write", &PythonEncoder::write)
            .def("put_frame", &PythonEncoder::put_frame);

    py::class_<PythonEncoderAsync>(m, "EncoderAsync")
            .def(py::init<>())
            .def("init", &PythonEncoderAsync::init)
            .def("enable_av_debug_log", &PythonEncoderAsync::enable_av_debug_log)
            //.def("close", &PythonEncoder::close)
            .def("release", &PythonEncoderAsync::close)
            .def("write", &PythonEncoderAsync::write);
            //.def("put_frame", &PythonEncoder::put_frame);

}



