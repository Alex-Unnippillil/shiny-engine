"""Create original RGB AVI + PCM audio fixtures, without downloads or encoders."""
from pathlib import Path
import math
import struct
import sys


def chunk(kind: bytes, data: bytes) -> bytes:
    return kind + struct.pack('<I', len(data)) + data + (b'\0' if len(data) & 1 else b'')


def stream_header(kind, codec, scale, rate, length, buffer, sample, width=0, height=0):
    return struct.pack('<4s4sIHHIIIIIIIIhhhh', kind, codec, 0, 0, 0, 0,
                       scale, rate, 0, length, buffer, 0xffffffff, sample, 0, 0, width, height)


def make(folder: Path):
    folder.mkdir(parents=True, exist_ok=True)
    width, height, fps, count, hz = 160, 96, 30, 600, 48000
    size = width * height * 3
    main = struct.pack('<14I', round(1e6 / fps), size * fps + hz * 2, 0, 0x10,
                       count, 0, 2, size, width, height, 0, 0, 0, 0)
    bitmap = struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0, size, 0, 0, 0, 0)
    video = chunk(b'LIST', b'strl' + chunk(b'strh', stream_header(b'vids', b'DIB ', 1, fps, count, size, 0, width, height)) + chunk(b'strf', bitmap))
    audioformat = struct.pack('<HHIIHH', 1, 1, hz, hz * 2, 2, 16)
    audio = chunk(b'LIST', b'strl' + chunk(b'strh', stream_header(b'auds', b'\0' * 4, 2, hz * 2, count * hz // fps, hz * 2 // fps, 2)) + chunk(b'strf', audioformat))
    headers = chunk(b'LIST', b'hdrl' + chunk(b'avih', main) + video + audio)
    frames, index, offset = [], [], 4
    base = bytearray(bytes([48, 96, 144]) * (width * height))
    for n in range(count):
        pixels = base.copy()
        # A small moving square, leaving the large center patch constant for pixel tests.
        for y in range(10):
            for x in range(10):
                p = ((height - 1 - y) * width + (n + x) % width) * 3
                pixels[p:p + 3] = b'\x20\xd0\xe0'
        pcm = b''.join(struct.pack('<h', int(1000 * math.sin(2 * math.pi * 440 * (n * hz // fps + s) / hz))) for s in range(hz // fps))
        for kind, data in ((b'00db', bytes(pixels)), (b'01wb', pcm)):
            entry = chunk(kind, data)
            frames.append(entry)
            index.append(struct.pack('<4sIII', kind, 0x10, offset, len(data)))
            offset += len(entry)
    riff = b'AVI ' + headers + chunk(b'LIST', b'movi' + b''.join(frames)) + chunk(b'idx1', b''.join(index))
    (folder / 'moving-original.avi').write_bytes(chunk(b'RIFF', riff))
    (folder / 'captions.srt').write_text('1\n00:00:00,000 --> 00:00:19,000\nOriginal synthetic VLC integration fixture\n', encoding='utf-8')
    print(folder / 'moving-original.avi')


if __name__ == '__main__':
    make(Path(sys.argv[1]))
