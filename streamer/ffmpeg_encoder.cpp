#include "ffmpeg_encoder.h"

extern "C" {
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libavutil/hwcontext.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
}

#include <iostream>


static AVBufferRef *vaapi_hw_device_ctx = NULL;

static int set_hwframe_ctx(AVCodecContext *ctx, AVBufferRef *hw_device_ctx, int width, int height)
{
    AVBufferRef *hw_frames_ref;
    AVHWFramesContext *frames_ctx = NULL;
    int err = 0;

    if (!(hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx))) {
        fprintf(stderr, "Failed to create VAAPI frame context.\n");
        return -1;
    }
    frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format    = AV_PIX_FMT_VAAPI;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width     = width;
    frames_ctx->height    = height;
    frames_ctx->initial_pool_size = 20;
    if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        fprintf(stderr, "Failed to initialize VAAPI frame context.");
                        //"Error code: %s\n",av_err2str(err));
        av_buffer_unref(&hw_frames_ref);
        return err;
    }
    ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!ctx->hw_frames_ctx)
        err = AVERROR(ENOMEM);

    av_buffer_unref(&hw_frames_ref);
    return err;
}


static void initialize_avformat_context(AVFormatContext **fctx, const char *format_name)
{
    int ret = avformat_alloc_output_context2(fctx, nullptr, format_name, nullptr);
    if (ret < 0)
    {
        std::cout << "Could not allocate output format context!" << std::endl;
        exit(1);
    }
}


static int initialize_io_context(AVFormatContext *fctx, const char *output)
{
    int ret = 0;
    if (!(fctx->oformat->flags & AVFMT_NOFILE))   {
        ret = avio_open2(&fctx->pb, output, AVIO_FLAG_WRITE, nullptr, nullptr);
        if (ret < 0)  {
            printf("Could not open output IO context!\n");
            return ret;
        }
    }
    return ret;
}


static void set_codec_params(AVCodec *codec, AVPixelFormat pix_fmt, AVFormatContext *fctx, AVCodecContext *codec_ctx, int width, int height, int fps, int bitrate)
{

    int frame_rate=(int)(fps+0.5);
    int  frame_rate_base=1;
    while (fabs(((double)frame_rate/frame_rate_base) - fps) > 0.001){
        frame_rate_base*=10;
        frame_rate=(int)(fps*frame_rate_base + 0.5);
    }

    codec_ctx->time_base.den = frame_rate;
    codec_ctx->time_base.num = frame_rate_base;
    /* adjust time base for supported framerates */
    if(codec && codec->supported_framerates){
        const AVRational *p= codec->supported_framerates;
        AVRational req = {frame_rate, frame_rate_base};
        const AVRational *best=NULL;
        AVRational best_error= {INT_MAX, 1};
        for(; p->den!=0; p++){
            AVRational error= av_sub_q(req, *p);
            if(error.num <0) error.num *= -1;
            if(av_cmp_q(error, best_error) < 0){
                best_error= error;
                best= p;
            }
        }
        if (best == NULL) {
            printf("Error in setting codec framerate\n");
        }

        codec_ctx->time_base.den= best->num;
        codec_ctx->time_base.num= best->den;
    }


    codec_ctx->codec_tag = 0;
    codec_ctx->codec_id = codec_ctx->codec->id;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->gop_size = 15;
    codec_ctx->pix_fmt = pix_fmt; //AV_PIX_FMT_VAAPI;//AV_PIX_FMT_YUV444P;
    codec_ctx->framerate = av_inv_q(codec_ctx->time_base);

    codec_ctx->bit_rate = bitrate;
    if (fctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

}


static int initialize_codec(AVStream *stream, AVCodecContext *codec_ctx, AVCodec *codec, const std::map<std::string, std::string> &codec_options)
{
    int ret;
    ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
    if (ret < 0)
    {
        std::cout << "Could not initialize stream codec parameters!" << std::endl;
        exit(1);
    }

    AVDictionary *av_codec_options = nullptr;
    for(auto it = codec_options.begin(); it != codec_options.end(); ++it) {
        //std::cout << it->first << " " << it->second << "\n";
        av_dict_set(&av_codec_options, it->first.c_str(), it->second.c_str(), 0);
    }

    // open video encoder
    ret = avcodec_open2(codec_ctx, codec, &av_codec_options);
    if (ret < 0) {
        fprintf(stderr, "Encoder: Could not open video encoder!\n");
        return 1;
    }

    if (codec_ctx->nb_coded_side_data) {
        int i;

        for (i = 0; i < codec_ctx->nb_coded_side_data; i++) {
            const AVPacketSideData *sd_src = &codec_ctx->coded_side_data[i];
            uint8_t *dst_data;

            dst_data = av_stream_new_side_data(stream, sd_src->type, sd_src->size);
            if (!dst_data)
                return AVERROR(ENOMEM);
            memcpy(dst_data, sd_src->data, sd_src->size);
        }
    }


    av_dict_free(&av_codec_options);

    return 0;

}


static SwsContext *initialize_sample_scaler(AVPixelFormat pix_fmt, int inp_width, int inp_height,
                                            int stream_width, int stream_height)
{
    SwsContext *swsctx = sws_getContext(inp_width, inp_height, AV_PIX_FMT_BGR24,
                                        stream_width, stream_height, pix_fmt, SWS_FAST_BILINEAR,
                                        nullptr, nullptr, nullptr);
    if (!swsctx)
    {
        std::cout << "Could not initialize sample scaler!" << std::endl;
    }

    return swsctx;
}


static AVFrame *set_frame_buffer(AVPixelFormat pix_fmt, int width, int height, std::vector<uint8_t> &framebuf)
{
    AVFrame *frame = av_frame_alloc();

    av_image_fill_arrays(frame->data, frame->linesize, framebuf.data(), pix_fmt, width, height, 1);

    frame->width = width;
    frame->height = height;
    frame->format = static_cast<int>(pix_fmt);
    frame->pts = 0;

    return frame;
}




static AVCodec *find_encoder_by_name(const char *name, enum AVMediaType type)
{
    const AVCodecDescriptor *desc;
    const char *codec_string = "encoder";
    AVCodec *codec;

    codec = avcodec_find_encoder_by_name(name);
    if(!codec) {
        desc = avcodec_descriptor_get_by_name(name);
        if(desc) {
            codec = avcodec_find_encoder(desc->id);
            if (codec)
                av_log(NULL, AV_LOG_VERBOSE, "Matched %s '%s' for codec '%s'.\n",
                       codec_string, codec->name, desc->name);
        }
    }

    if (!codec) {
        av_log(NULL, AV_LOG_FATAL, "Unknown %s '%s'\n", codec_string, name);
        exit(1);
    }

    if (codec->type != type) {
        av_log(NULL, AV_LOG_FATAL, "Invalid %s type '%s'\n", codec_string, name);
        exit(1);
    }

    return codec;
}


static int write_frame(AVCodecContext *codec_ctx, AVFormatContext *fmt_ctx, AVFrame *frame, bool rescale)
{
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    int ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending frame to codec context!\n");
        return ret;
    }

    while(1) {
        ret = avcodec_receive_packet(codec_ctx, &pkt);

        if (ret == AVERROR(EAGAIN)) {
            break;
        }

        if (ret < 0) {

            fprintf(stderr, "Error receiving packet from codec context!\n" );
            return ret;
        }
//        printf("%d %d %d %d\n", fmt_ctx->streams[0]->time_base.num, fmt_ctx->streams[0]->time_base.den,
//                codec_ctx->time_base.num, codec_ctx->time_base.den);

//        if (pkt.duration == 0) {
//            pkt.duration = av_rescale_q(1, codec_ctx->time_base, fmt_ctx->streams[0]->time_base);
//        }
        //printf("dur %ld \n", pkt.duration);

        //printf("writing !\n");
        if(rescale) {
            av_packet_rescale_ts(&pkt, codec_ctx->time_base, fmt_ctx->streams[0]->time_base);
        }

        //printf("dur after %ld \n", pkt.duration);

        av_interleaved_write_frame(fmt_ctx, &pkt);
        av_packet_unref(&pkt);

    }

    return ret;

}

void Encoder::put_frame(const uint8_t *data, double duration)
{
    if (init_failure) {
        return;
    }

    if(!swsctx) {
        AVPixelFormat pix_fmt = out_codec_ctx->pix_fmt;
        if (use_vaapi) {
            pix_fmt = AV_PIX_FMT_NV12;
        }
        swsctx = initialize_sample_scaler(pix_fmt, config.src_width, config.src_height, config.dst_width, config.dst_height);
    }
    if (!swsctx) {
        printf("Encoder : failed to initialize scaler\n");
        return;
    }

    const int stride[] = {static_cast<int>(config.src_width*3)};

    sws_scale(swsctx, &data, stride, 0, config.src_height, frame->data, frame->linesize);

    AVFrame *hw_frame = nullptr;

    if (use_vaapi) {

        if (!(hw_frame = av_frame_alloc())) {
            exit(0);
        }

        if (av_hwframe_get_buffer(out_codec_ctx->hw_frames_ctx, hw_frame, 0) < 0) {
            printf("aia!\n");
            exit(0);
        }
        if (!hw_frame->hw_frames_ctx) {
            printf("aia!\n");
            exit(0);
        }
        if ((av_hwframe_transfer_data(hw_frame, frame, 0)) < 0) {
            fprintf(stderr, "Error while transferring frame data to surface.");
            exit(0);
        }

        if(duration != 0.0) {
            frame->pts += (int64_t) round(duration);
            hw_frame->pts = frame->pts;
            write_frame(out_codec_ctx, ofmt_ctx, hw_frame, false);
        } else {
            frame->pts += 1;
            hw_frame->pts = frame->pts;
            write_frame(out_codec_ctx, ofmt_ctx, hw_frame, true);
        }

        av_frame_free(&hw_frame);
    }

    else {
        if(duration != 0.0) {
            frame->pts += (int64_t) round(duration);
            write_frame(out_codec_ctx, ofmt_ctx, frame, false);
        } else {
            frame->pts += 1;
            write_frame(out_codec_ctx, ofmt_ctx, frame, true);
        }
    }
}


bool Encoder::stream_init()
{
    auto codec_name = config.codec_name;
    auto codec_params = config.codec_params;

    printf("init network\n");
    avformat_network_init();

    initialize_avformat_context(&ofmt_ctx, "flv");

    int init = initialize_io_context(ofmt_ctx, config.output.c_str());
    if (init != 0) { // failure to initialize
        init_failure = true;
        return false;
    }

    out_codec = find_encoder_by_name(codec_name.c_str(), AVMEDIA_TYPE_VIDEO);
    AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;

    std::size_t vaapi_found = config.codec_name.find("vaapi");
    if (vaapi_found !=std::string::npos) {
        use_vaapi = true;
        pix_fmt = AV_PIX_FMT_NV12;
        printf("requested vaapi codec\n");
        int err = av_hwdevice_ctx_create(&vaapi_hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI,  NULL, NULL, 0);
        if (err < 0) {
            fprintf(stderr, "Failed to create a VAAPI device.");
        }
    }


    out_stream = avformat_new_stream(ofmt_ctx, nullptr);
    out_codec_ctx = avcodec_alloc_context3(out_codec);
    if (use_vaapi) {
        set_codec_params(out_codec, AV_PIX_FMT_VAAPI, ofmt_ctx, out_codec_ctx, config.dst_width, config.dst_height, config.fps, config.bitrate);

        if (set_hwframe_ctx(out_codec_ctx, vaapi_hw_device_ctx, config.dst_width, config.dst_height) < 0) {
            fprintf(stderr, "Failed to set hwframe context.\n");
        }
    } else {
        set_codec_params(out_codec, pix_fmt, ofmt_ctx, out_codec_ctx, config.dst_width, config.dst_height, config.fps, config.bitrate);
    }


    out_stream->time_base = out_codec_ctx->time_base; //will be set afterwards by avformat_write_header to 1/1000

    initialize_codec(out_stream, out_codec_ctx, out_codec, codec_params);
    printf("codec initialized\n");

    out_stream->codecpar->extradata_size = out_codec_ctx->extradata_size;
    out_stream->codecpar->extradata = (uint8_t*) av_mallocz(out_codec_ctx->extradata_size);
    memcpy(out_stream->codecpar->extradata, out_codec_ctx->extradata, out_codec_ctx->extradata_size);

    av_dump_format(ofmt_ctx, 0, config.output.c_str(), 1);

    framebuf.resize(av_image_get_buffer_size(pix_fmt, config.dst_width, config.dst_height, 1));
    frame = set_frame_buffer(pix_fmt, config.dst_width, config.dst_height, framebuf);

    out_stream->avg_frame_rate = out_codec_ctx->framerate;

    int ret = avformat_write_header(ofmt_ctx, nullptr);
    if (ret < 0)
    {
        std::cout << "Encoder: Could not write header!" << std::endl;
        return false;
    }

    printf("encoder initialized\n");
    return true;
}



bool Encoder::file_init()
{
    avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, config.output.c_str());
    out_codec = find_encoder_by_name(config.codec_name.c_str(), AVMEDIA_TYPE_VIDEO);

    AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;
    std::size_t vaapi_found = config.codec_name.find("vaapi");
    if (vaapi_found !=std::string::npos) {
        use_vaapi = true;
        pix_fmt = AV_PIX_FMT_NV12;
        printf("requested vaapi codec\n");
        int err = av_hwdevice_ctx_create(&vaapi_hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI,  NULL, NULL, 0);
        if (err < 0) {
            fprintf(stderr, "Failed to create a VAAPI device.");
        }
    }


    out_stream = avformat_new_stream(ofmt_ctx, out_codec);
    out_codec_ctx = avcodec_alloc_context3(out_codec);
    if (use_vaapi) {
        set_codec_params(out_codec, AV_PIX_FMT_VAAPI, ofmt_ctx, out_codec_ctx, config.dst_width, config.dst_height, config.fps, config.bitrate);

        if (set_hwframe_ctx(out_codec_ctx, vaapi_hw_device_ctx, config.dst_width, config.dst_height) < 0) {
            fprintf(stderr, "Failed to set hwframe context.\n");
        }
    } else {
        set_codec_params(out_codec, pix_fmt, ofmt_ctx, out_codec_ctx, config.dst_width, config.dst_height, config.fps, config.bitrate);
    }

    out_stream->time_base = out_codec_ctx->time_base; //will be set afterwards by avformat_write_header to 1/1000

    //printf("num of cp = %lu\n", config.codec_params.size());
    initialize_codec(out_stream, out_codec_ctx, out_codec, config.codec_params);
    printf("codec initialized\n");

    out_stream->codecpar->extradata_size = out_codec_ctx->extradata_size;
    out_stream->codecpar->extradata = (uint8_t*) av_mallocz(out_codec_ctx->extradata_size);
    memcpy(out_stream->codecpar->extradata, out_codec_ctx->extradata, out_codec_ctx->extradata_size);

    if ((avio_open2(&ofmt_ctx->pb, config.output.c_str(), AVIO_FLAG_WRITE,
                          nullptr,
                          nullptr)) < 0) {
        std::cout << "Encoder: avio open2 error!" << std::endl;
        return false;
    }
    av_dump_format(ofmt_ctx, 0, config.output.c_str(), 1);

    framebuf.resize(av_image_get_buffer_size(pix_fmt, config.dst_width, config.dst_height, 1));

    frame = set_frame_buffer(pix_fmt, config.dst_width, config.dst_height, framebuf);

    out_stream->avg_frame_rate = out_codec_ctx->framerate;

    int ret = avformat_write_header(ofmt_ctx, nullptr);
    if (ret < 0)
    {
        std::cout << "Encoder: Could not write header!" << std::endl;
        return false;
    }

    printf("encoder initialized\n");
    return true;
}


bool Encoder::init(const EncoderConfig &config_)
{
    config = config_;
    if (config.mode == ENCODER_MODE::FILE) {
        return file_init();
    } else if (config.mode == ENCODER_MODE::STREAM) {
        return stream_init();
    } else {
        printf("Encoder Error: unknown mode");
        return false;
    }
    return true;
}


void Encoder::enable_av_debug_log()
{
    av_log_set_level(AV_LOG_DEBUG);
}


Encoder::Encoder()
{
    ofmt_ctx = nullptr;
    out_codec = nullptr;
    out_stream = nullptr;
    out_codec_ctx = nullptr;
    frame = nullptr;
    swsctx = nullptr;
    init_failure = false;

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    avdevice_register_all();

}

void Encoder::close()
{
    if(ofmt_ctx && out_codec_ctx) {
        av_write_trailer(ofmt_ctx);
    }

    if(frame) {
        av_frame_free(&frame);
        frame = nullptr;
    }

    if(swsctx) {
        sws_freeContext(swsctx);
        swsctx = nullptr;
    }


    if(out_codec_ctx) {
        avcodec_close(out_codec_ctx);
        avcodec_free_context(&out_codec_ctx);
        out_codec_ctx = nullptr;
    }

    if(ofmt_ctx) {
        avio_close(ofmt_ctx->pb);
        avformat_free_context(ofmt_ctx);
        ofmt_ctx = nullptr;
    }
}

Encoder::~Encoder()
{
    close();
}

