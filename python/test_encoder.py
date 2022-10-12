from rtmp_streaming import EncoderConfig, Encoder
import cv2
import sys


fin = sys.argv[1]

print(fin)


cfg = EncoderConfig()
cfg.set_mode_file()
cfg.output = 'test.mkv'
cfg.codec_name = 'h264_nvenc' #
cfg.enc_width = 640
cfg.enc_height = 480
cfg.enc_fps = 30
#cp = {}
#cp["profile"] = "high"
#cp["preset"]  = "medium"
#cfg.codec_params = cp
cfg.codec_params["profile"] = "high"
cfg.codec_params["preset"]  = "medium"

# cfg.codec_params["tune"] = "zerolatency"
cfg.enc_bitrate = 1000000

enc = Encoder()
ec_init = False


def tweak_encoder_cfg(frame, enc_cfg):
    src_w = frame.shape[1]
    src_h = frame.shape[0]

    enc_cfg.source_width = src_w
    enc_cfg.source_height = src_h
    return enc_cfg
    


cap = cv2.VideoCapture(fin)
while(cap.isOpened()):
  # Capture frame-by-frame
  ret, frame = cap.read()
  if ret == True:

    if not ec_init:
        cfg = tweak_encoder_cfg(frame, cfg)
        ok = enc.init(cfg)
        print('enc init ok', ok)
        if not ok:
            exit()

        else:
            ec_init = True

    enc.put_frame(frame, 0.0)  
    # Display the resulting frame
    cv2.imshow('Frame',frame)

    
    # Press Q on keyboard to  exit
    if cv2.waitKey(25) & 0xFF == ord('q'):
      break

  # Break the loop
  else: 
    break

# When everything done, release the video capture object
cap.release()
enc.close()


# Closes all the frames
cv2.destroyAllWindows()


