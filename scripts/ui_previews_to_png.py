"""Convert exact software-rendered RGB previews to PNG using the standard library."""
import struct
import sys
from pathlib import Path
import zlib


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind+data) & 0xffffffff)


for source in Path(sys.argv[1]).glob("*.ppm"):
    with source.open("rb") as f:
        if f.readline() != b"P6\n":
            raise ValueError("Expected rendered P6 preview")
        w, h = map(int, f.readline().split())
        if f.readline() != b"255\n":
            raise ValueError("Expected RGB8")
        rgb = f.read()
    if len(rgb) != w*h*3:
        raise ValueError("Truncated preview")
    scanlines = b"".join(b"\0" + rgb[y*w*3:(y+1)*w*3] for y in range(h))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    source.with_suffix(".png").write_bytes(png + chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b""))
