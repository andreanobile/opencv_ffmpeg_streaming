import numpy as np
import cv2
import time
from rtmp_streaming import StreamerConfig, Streamer


def main():

    cap = cv2.VideoCapture(0)
    ret, frame = cap.read()

    sc = StreamerConfig()
    sc.source_width = frame.shape[1]
    sc.source_height = frame.shape[0]
    sc.stream_width = 640
    sc.stream_height = 480
    sc.stream_fps = 25
    sc.stream_bitrate = 1000000
    sc.stream_profile = 'main' #'high444' # 'main'
    sc.stream_server = 'rtmp://localhost/live/mystream'


    streamer = Streamer()
    streamer.init(sc)
    streamer.enable_av_debug_log()


    prev = time.time()

    show_cap = True

    while(True):

        ret, frame = cap.read()
        now = time.time()
        duration = now - prev
        streamer.stream_frame_with_duration(frame, int(duration*1000))
        prev = now
        if show_cap:
            cv2.imshow('frame', frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break


    cap.release()
    if show_cap:
        cv2.destroyAllWindows()



if __name__ == "__main__":
    main()
    



