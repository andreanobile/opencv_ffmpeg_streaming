#include <string>
#include <opencv2/opencv.hpp>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>


#include "streamer/streamer.hpp"

using namespace streamer;


int main(int argc, char *argv[])
{
    if(argc != 2) {
        printf("must provide one command argument with the video file or stream to open\n");
        return 1;
    }
    std::string video_fname;
    video_fname = std::string(argv[1]);

    cv::VideoCapture video_capture(video_fname, cv::CAP_FFMPEG);
    if(!video_capture.isOpened()) {
        fprintf(stderr, "could not open video %s\n", video_fname.c_str());
        video_capture.release();
        return 1;
    }

    int cap_frame_width = video_capture.get(cv::CAP_PROP_FRAME_WIDTH);
    int cap_frame_height = video_capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    int cap_fps = video_capture.get(cv::CAP_PROP_FPS);
    printf("video info w = %d, h = %d, fps = %d\n", cap_frame_width, cap_frame_height, cap_fps);

    int bitrate = 500000;
    Streamer streamer;
    StreamerConfig streamer_config(cap_frame_width, cap_frame_height,
                                   320, 180,
                                   cap_fps, bitrate, "high444", "rtmp://localhost/live/mystream");

    streamer.enable_av_debug_log();
    streamer.init(streamer_config);

    cv::Mat read_frame;
    bool ok = video_capture.read(read_frame);
    while(ok) {
        streamer.stream_frame(read_frame);
        ok = video_capture.read(read_frame);
    }
    video_capture.release();

    return 0;
}
