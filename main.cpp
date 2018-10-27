#include "streamer/streamer.hpp"

#include <string>
#include <opencv2/opencv.hpp>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <chrono>

using namespace streamer;


class MovingAverage
{
    int size;
    int pos;
    bool crossed;
    std::vector<double> v;

public:
    explicit MovingAverage(int sz)
    {
        size = sz;
        v.resize(size);
        pos = 0;
        crossed = false;
    }

    void add_value(double value)
    {
        v[pos] = value;
        pos++;
        if(pos == size) {
            pos = 0;
            crossed = true;
        }
    }

    double get_average()
    {
        double avg = 0.0;
        int last = crossed ? size : pos;
        int k=0;
        for(k=0;k<last;k++) {
            avg += v[k];
        }
        return avg / (double)last;
    }
};


static void add_delay(size_t streamed_frames, size_t fps, double elapsed, double avg_frame_time)
{
    //compute min number of frames that should have been streamed based on fps and elapsed
    double dfps = fps;
    size_t min_streamed = (size_t) (dfps*elapsed);
    size_t min_plus_margin = min_streamed + 2;

    if(streamed_frames > min_plus_margin) {
        size_t excess = streamed_frames - min_plus_margin;
        double dexcess = excess;

        //add a delay ~ excess*processing_time
//#define SHOW_DELAY
#ifdef SHOW_DELAY
        double delay = dexcess*avg_frame_time*1000000.0;
        printf("frame %07lu adding delay %.4f\n", streamed_frames, delay);
        printf("avg fps = %.2f\n", streamed_frames/elapsed);
#endif
        usleep(dexcess*avg_frame_time*1000000.0);
    }
}

void process_frame(const cv::Mat &in, cv::Mat &out)
{
    in.copyTo(out);
}

int main(int argc, char *argv[])
{
    if(argc != 2) {
        printf("must provide one command argument with the video file or stream to open\n");
        return 1;
    }
    std::string video_fname;
    video_fname = std::string(argv[1]);
    cv::VideoCapture video_capture;
    bool from_camera = false;
    if(video_fname == "0") {
        video_capture = cv::VideoCapture(0);
        from_camera = true;
    } else {
        video_capture=  cv::VideoCapture(video_fname, cv::CAP_FFMPEG);
    }


    if(!video_capture.isOpened()) {
        fprintf(stderr, "could not open video %s\n", video_fname.c_str());
        video_capture.release();
        return 1;
    }

    int cap_frame_width = video_capture.get(cv::CAP_PROP_FRAME_WIDTH);
    int cap_frame_height = video_capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    int cap_fps = video_capture.get(cv::CAP_PROP_FPS);
    printf("video info w = %d, h = %d, fps = %d\n", cap_frame_width, cap_frame_height, cap_fps);

    int stream_fps = cap_fps;

    int bitrate = 500000;
    Streamer streamer;
    StreamerConfig streamer_config(cap_frame_width, cap_frame_height,
                                   640, 360,
                                   stream_fps, bitrate, "high444", "rtmp://localhost/live/mystream");

    streamer.enable_av_debug_log();
    streamer.init(streamer_config);

    size_t streamed_frames = 0;

    std::chrono::high_resolution_clock clk;
    std::chrono::high_resolution_clock::time_point time_start = clk.now();
    std::chrono::high_resolution_clock::time_point time_stop = time_start;
    std::chrono::high_resolution_clock::time_point time_prev = time_start;

    MovingAverage moving_average(10);
    double avg_frame_time;

    cv::Mat read_frame;
    cv::Mat proc_frame;
    bool ok = video_capture.read(read_frame);

    std::chrono::duration<double> elapsed_time = std::chrono::duration_cast<std::chrono::duration<double>>(time_stop - time_start);
    std::chrono::duration<double> frame_time = std::chrono::duration_cast<std::chrono::duration<double>>(time_stop - time_prev);

    while(ok) {
        process_frame(read_frame, proc_frame);
        if(!from_camera) {
            streamer.stream_frame(proc_frame);
        } else {

            streamer.stream_frame(proc_frame, frame_time.count()*streamer.inv_stream_timebase);
        }

        time_stop = clk.now();
        elapsed_time = std::chrono::duration_cast<std::chrono::duration<double>>(time_stop - time_start);
        frame_time = std::chrono::duration_cast<std::chrono::duration<double>>(time_stop - time_prev);

        if(!from_camera) {
            streamed_frames++;
            moving_average.add_value(frame_time.count());
            avg_frame_time = moving_average.get_average();
            add_delay(streamed_frames, stream_fps, elapsed_time.count(), avg_frame_time);
        }

        ok = video_capture.read(read_frame);
        time_prev = time_stop;
    }
    video_capture.release();

    return 0;
}
