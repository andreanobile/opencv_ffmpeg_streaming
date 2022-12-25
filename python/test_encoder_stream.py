from rtmp_streaming import EncoderConfig, Encoder
import cv2
import sys
import time


fin = sys.argv[1]

print(fin)


cfg = EncoderConfig()
cfg.set_mode_stream()
cfg.output = 'rtmp://80.211.254.38/homestream78356/test'
cfg.codec_name = 'h264_vaapi' #'h264_nvenc' #
cfg.enc_width = 1280
cfg.enc_height = 720
cfg.enc_fps = 30
#cp = {}
#cp["profile"] = "high"
#cp["preset"]  = "medium"
#cfg.codec_params = cp
cfg.codec_params["profile"] = "baseline"
cfg.codec_params["preset"]  = "medium"

# cfg.codec_params["tune"] = "zerolatency"
cfg.enc_bitrate = 2000000

enc = Encoder()
#enc.enable_av_debug_log()

ec_init = False


def tweak_encoder_cfg(frame, enc_cfg):
    src_w = frame.shape[1]
    src_h = frame.shape[0]

    enc_cfg.source_width = src_w
    enc_cfg.source_height = src_h
    return enc_cfg
    

if fin.isnumeric():
    fin = int(fin)
    
cap = cv2.VideoCapture(fin)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
prev = time.time()
while(cap.isOpened()):
  # Capture frame-by-frame
  ret, frame = cap.read()
  if ret == True:

    if not ec_init:
        cfg = tweak_encoder_cfg(frame, cfg)
        ok = enc.init(cfg)
        print('enc init ok', ok)
        if not ok:
            print('encoder init not ok')
            exit()

        else:
            ec_init = True
    cur = time.time()
    duration = cur-prev
    print('duration', duration)
    #enc.put_frame(frame, duration)
    #enc.put_frame(frame, 100)
    prev = cur
    # Display the resulting frame
    cv2.imshow('Frame',frame)
    # Press Q on keyboard to  exit
    if cv2.waitKey(2) & 0xFF == ord('q'):
      break

  # Break the loop
  else: 
    break

# When everything done, release the video capture object
cap.release()
enc.close()


# Closes all the frames
cv2.destroyAllWindows()


