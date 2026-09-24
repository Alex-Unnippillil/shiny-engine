"""Encode one JPEG as a ten-second, silent MJPEG AVI for a README capture.

This is a documentation still-image clip, not telescope video or a test fixture.
No encoder dependency is needed: MJPEG stores the already encoded JPEG frames.
"""
from pathlib import Path
import struct
import sys


def chunk(kind: bytes, data: bytes) -> bytes:
    return kind + struct.pack('<I', len(data)) + data + (b'\0' if len(data) & 1 else b'')


def make(source: Path, output: Path) -> None:
    if output.exists():
        raise ValueError('Refusing to replace an existing media file')
    jpeg = source.read_bytes()
    if not (jpeg.startswith(b'\xff\xd8') and jpeg.endswith(b'\xff\xd9')) or len(jpeg) > 4 * 1024 * 1024:
        raise ValueError('A bounded JPEG produced by capture-readme.ps1 is required')
    width, height, fps, count = 1280, 742, 24, 240
    main = struct.pack('<14I', round(1e6 / fps), len(jpeg) * fps, 0, 0x10,
                       count, 0, 1, len(jpeg), width, height, 0, 0, 0, 0)
    stream = struct.pack('<4s4sIHHIIIIIIIIhhhh', b'vids', b'MJPG', 0, 0, 0, 0,
                         1, fps, 0, count, len(jpeg), 0xffffffff, 0, 0, 0, width, height)
    bitmap = struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24,
                         int.from_bytes(b'MJPG', 'little'), width * height * 3, 0, 0, 0, 0)
    video = chunk(b'LIST', b'strl' + chunk(b'strh', stream) + chunk(b'strf', bitmap))
    header = chunk(b'LIST', b'hdrl' + chunk(b'avih', main) + video)
    frame = chunk(b'00dc', jpeg)
    index = b''.join(struct.pack('<4sIII', b'00dc', 0x10, 4 + i * len(frame), len(jpeg))
                     for i in range(count))
    riff = b'AVI ' + header + chunk(b'LIST', b'movi' + frame * count) + chunk(b'idx1', index)
    output.write_bytes(chunk(b'RIFF', riff))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('Usage: make-still-clip.py SOURCE.jpg OUTPUT.avi')
    make(Path(sys.argv[1]), Path(sys.argv[2]))
