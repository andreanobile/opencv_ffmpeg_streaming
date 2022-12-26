#ifndef FFMPEG_ENCODER_H
#define FFMPEG_ENCODER_H

//#include <opencv2/core.hpp>

#include <string>
#include <vector>
#include <map>

struct AVFormatContext;
struct AVCodec;
struct AVStream;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;

enum class ENCODER_MODE
{
    STREAM = 0,
    FILE
};

struct EncoderConfig
{


    int src_width = 0;
    int src_height = 0;
    int dst_width = 0;
    int dst_height = 0;
    int fps = 0;
    int bitrate = 0;
    std::string codec_name;
    std::map<std::string, std::string> codec_params;
    std::string output;

    ENCODER_MODE mode = ENCODER_MODE::FILE;


    void set_mode_file()
    {
        mode = ENCODER_MODE::FILE;
    }

    void set_mode_stream()
    {
        mode = ENCODER_MODE::STREAM;
    }

};

using Packet = std::vector<uint8_t>;

class Encoder
{
    AVFormatContext *ofmt_ctx;
    AVCodec *out_codec;
    AVStream *out_stream;
    AVCodecContext *out_codec_ctx;
    AVFrame *frame;
    SwsContext *swsctx;
    std::vector<uint8_t> framebuf;
    bool init_failure;
    bool use_vaapi = false;

    //std::string codec_name;
    //std::map<std::string, std::string> codec_params;

    bool stream_init();
    bool file_init();

public:
    Encoder();
    virtual ~Encoder();

    bool init(const EncoderConfig &config);
    bool open(const EncoderConfig &config);


    Packet encode(const uint8_t *data);
    void put_frame(const uint8_t *data, double duration=0.0);


    void close();
    void enable_av_debug_log();
    EncoderConfig config;

};

#endif // FFMPEG_ENCODER_H







