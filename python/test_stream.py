import numpy as np
import cv2
import time
from rtmp_streaming import StreamerConfig, Streamer


sc = StreamerConfig()
sc.source_width = 640
sc.source_height = 480
sc.stream_width = 640
sc.stream_height = 480
sc.stream_fps = 16
sc.stream_bitrate = 1000000
sc.stream_profile = 'main' #'high444' # 'main'
sc.stream_server = 'rtmp://localhost/live/mystream'


streamer = Streamer()
streamer.init(sc)
#streamer.enable_av_debug_log()

cap = cv2.VideoCapture(0)


prev = time.time()
while(True):
    # Capture frame-by-frame
    ret, frame = cap.read()

    
    #gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    now = time.time()
    duration = now - prev
    streamer.stream_frame_with_duration(frame, int(duration*1000))
    
    
    prev = now
    #cv2.imshow('frame', frame)
    #if cv2.waitKey(1) & 0xFF == ord('q'):
    #    break
    #print(frame.shape)

# When everything done, release the capture
cap.release()
cv2.destroyAllWindows()





    



