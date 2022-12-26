
from rtmp_streaming import Encoder as FFEncoder, EncoderConfig

import av
from av import VideoFrame
from av.frame import Frame
from av.packet import Packet
import numpy as np
from aiortc.codecs.base import Encoder
from aiortc.mediastreams import convert_timebase, VIDEO_TIME_BASE
import math

from struct import pack
from typing import Iterator, List, Optional, Tuple

DEFAULT_BITRATE = 1000000  # 1 Mbps
MIN_BITRATE = 500000  # 500 kbps
MAX_BITRATE = 3000000  # 3 Mbps

MAX_FRAME_RATE = 30
PACKET_MAX = 1300

NAL_TYPE_FU_A = 28
NAL_TYPE_STAP_A = 24

NAL_HEADER_SIZE = 1
FU_A_HEADER_SIZE = 2
LENGTH_FIELD_SIZE = 2
STAP_A_HEADER_SIZE = NAL_HEADER_SIZE + LENGTH_FIELD_SIZE


def create_encoder_context(
    codec_name: str, width: int, height: int, bitrate: int
) -> Tuple[FFEncoder, bool]:

    config = EncoderConfig()
    config.codec_name = codec_name
    config.source_width = width
    config.source_height = height
    config.enc_width = min(width, 1280)
    config.enc_height = min(height, 720)
    config.enc_fps = MAX_FRAME_RATE
    print('creating codec %s bitrate = %d'%(codec_name, bitrate))
    config.enc_bitrate = bitrate
    config.codec_params = {
        #"profile": "baseline",
        "level": "31",
        "tune": "zerolatency",  # does nothing using h264_omx
    }

    codec = FFEncoder()
    #codec.enable_av_debug_log()
    codec.open(config)

    return codec, codec_name == "h264_omx"


class H264FFMpegEncoder(Encoder):
    def __init__(self, codec_name="h264_vaapi") -> None:
        self.buffer_data = b""
        self.buffer_pts: Optional[int] = None
        self.codec: Optional[av.CodecContext] = None
        self.codec_buffering = False
        self.__target_bitrate = DEFAULT_BITRATE
        self.codec_name = codec_name

    @staticmethod
    def _packetize_fu_a(data: bytes) -> List[bytes]:
        available_size = PACKET_MAX - FU_A_HEADER_SIZE
        payload_size = len(data) - NAL_HEADER_SIZE
        num_packets = math.ceil(payload_size / available_size)
        num_larger_packets = payload_size % num_packets
        package_size = payload_size // num_packets

        f_nri = data[0] & (0x80 | 0x60)  # fni of original header
        nal = data[0] & 0x1F

        fu_indicator = f_nri | NAL_TYPE_FU_A

        fu_header_end = bytes([fu_indicator, nal | 0x40])
        fu_header_middle = bytes([fu_indicator, nal])
        fu_header_start = bytes([fu_indicator, nal | 0x80])
        fu_header = fu_header_start

        packages = []
        offset = NAL_HEADER_SIZE
        while offset < len(data):
            if num_larger_packets > 0:
                num_larger_packets -= 1
                payload = data[offset : offset + package_size + 1]
                offset += package_size + 1
            else:
                payload = data[offset : offset + package_size]
                offset += package_size

            if offset == len(data):
                fu_header = fu_header_end

            packages.append(fu_header + payload)

            fu_header = fu_header_middle
        assert offset == len(data), "incorrect fragment data"

        return packages

    @staticmethod
    def _packetize_stap_a(
        data: bytes, packages_iterator: Iterator[bytes]
    ) -> Tuple[bytes, bytes]:
        counter = 0
        available_size = PACKET_MAX - STAP_A_HEADER_SIZE

        stap_header = NAL_TYPE_STAP_A | (data[0] & 0xE0)

        payload = bytes()
        try:
            nalu = data  # with header
            while len(nalu) <= available_size and counter < 9:
                stap_header |= nalu[0] & 0x80

                nri = nalu[0] & 0x60
                if stap_header & 0x60 < nri:
                    stap_header = stap_header & 0x9F | nri

                available_size -= LENGTH_FIELD_SIZE + len(nalu)
                counter += 1
                payload += pack("!H", len(nalu)) + nalu
                nalu = next(packages_iterator)

            if counter == 0:
                nalu = next(packages_iterator)
        except StopIteration:
            nalu = None

        if counter <= 1:
            return data, nalu
        else:
            return bytes([stap_header]) + payload, nalu

    @staticmethod
    def _split_bitstream(buf: bytes) -> Iterator[bytes]:
        # Translated from: https://github.com/aizvorski/h264bitstream/blob/master/h264_nal.c#L134
        i = 0
        while True:
            # Find the start of the NAL unit
            # NAL Units start with a 3-byte or 4 byte start code of 0x000001 or 0x00000001
            # while buf[i:i+3] != b'\x00\x00\x01':
            i = buf.find(b"\x00\x00\x01", i)
            if i == -1:
                return

            # Jump past the start code
            i += 3
            nal_start = i

            # Find the end of the NAL unit (end of buffer OR next start code)
            i = buf.find(b"\x00\x00\x01", i)
            if i == -1:
                yield buf[nal_start : len(buf)]
                return
            elif buf[i - 1] == 0:
                # 4-byte start code case, jump back one byte
                yield buf[nal_start : i - 1]
            else:
                yield buf[nal_start:i]

    @classmethod
    def _packetize(cls, packages: Iterator[bytes]) -> List[bytes]:
        packetized_packages = []

        packages_iterator = iter(packages)
        package = next(packages_iterator, None)
        while package is not None:
            if len(package) > PACKET_MAX:
                packetized_packages.extend(cls._packetize_fu_a(package))
                package = next(packages_iterator, None)
            else:
                packetized, package = cls._packetize_stap_a(package, packages_iterator)
                packetized_packages.append(packetized)

        return packetized_packages

    def _encode_frame(
        self, frame: av.VideoFrame, force_keyframe: bool
    ) -> Iterator[bytes]:
        if self.codec and (
            frame.width != self.codec.config.source_width
            or frame.height != self.codec.config.source_height
            # we only adjust bitrate if it changes by over 10%
            or abs(self.target_bitrate - self.codec.config.enc_bitrate) / self.codec.config.enc_bitrate
            > 0.1
        ):
            self.buffer_data = b""
            self.buffer_pts = None
            self.codec = None

        # reset the picture type, otherwise no B-frames are produced
        frame.pict_type = av.video.frame.PictureType.NONE

        if self.codec is None:
            self.codec, self.codec_buffering = create_encoder_context(
                self.codec_name,
                frame.width,
                frame.height,
                bitrate=self.__target_bitrate,
            )

        data_to_send = b""

        pkt = self.codec.encode(frame.format.name, frame.width, frame.height, frame.planes[0])

        data_to_send += bytes(pkt)

        if data_to_send:
            yield from self._split_bitstream(data_to_send)



    def encode(
        self, frame: Frame, force_keyframe: bool = False
    ) -> Tuple[List[bytes], int]:
        assert isinstance(frame, av.VideoFrame)
        packages = self._encode_frame(frame, force_keyframe)
        timestamp = convert_timebase(frame.pts, frame.time_base, VIDEO_TIME_BASE)
        return self._packetize(packages), timestamp

    def pack(self, packet: Packet) -> Tuple[List[bytes], int]:
        assert isinstance(packet, av.Packet)
        packages = self._split_bitstream(bytes(packet))
        timestamp = convert_timebase(packet.pts, packet.time_base, VIDEO_TIME_BASE)
        return self._packetize(packages), timestamp

    @property
    def target_bitrate(self) -> int:
        """
        Target bitrate in bits per second.
        """
        return self.__target_bitrate

    @target_bitrate.setter
    def target_bitrate(self, bitrate: int) -> None:
        bitrate = max(MIN_BITRATE, min(bitrate, MAX_BITRATE))
        self.__target_bitrate = bitrate


def noise_frame():
    frame = np.clip((np.random.randn(720 * 1280 * 3) + 0.5) * 255.0, 0, 255).astype(np.uint8).reshape(720, 1280, 3)
    return frame


def send_frame(codec, frame: VideoFrame):
    format_name = frame.format.name
    width = frame.width
    height = frame.height
    np_pkt = codec.encode(format_name, width, height, frame.planes[0])

    return np_pkt



def test():
    img = noise_frame()
    frame = VideoFrame.from_ndarray(img, format="bgr24")
    codec, codec_buffering = create_encoder_context('libx264', frame.width, frame.height, 1000000)
    for i in range(10):
        send_frame(codec, frame)


def main():
    test()

if __name__ == "__main__":
    main()


