"""Minimal PNG reader and writer, standard library only.

Reads the files the autotest runner writes (session-p4/autotest/capture, PIL ``Image.save``
of an xwd capture): 8-bit truecolour (colour type 2) or truecolour with alpha (type 6),
plus 8-bit greyscale (type 0) for test fixtures, non-interlaced, all five filter types.
Anything else is refused with ``PngError`` rather than guessed. The writer emits 8-bit RGB
with filter 0, which is all the tests and the diff images need.
"""
from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass
from pathlib import Path

SIGNATURE = b"\x89PNG\r\n\x1a\n"
_CHANNELS = {0: 1, 2: 3, 6: 4}


class PngError(ValueError):
    pass


@dataclass
class Image:
    """RGB image: ``rgb`` holds width*height*3 bytes, rows top to bottom."""
    width: int
    height: int
    rgb: bytes

    def pixel(self, x: int, y: int) -> tuple[int, int, int]:
        i = (y * self.width + x) * 3
        return self.rgb[i], self.rgb[i + 1], self.rgb[i + 2]

    def crop(self, x0: int, y0: int, x1: int, y1: int) -> "Image":
        """Half-open rectangle [x0, x1) x [y0, y1), clamped to the image."""
        x0, x1 = max(0, x0), min(self.width, x1)
        y0, y1 = max(0, y0), min(self.height, y1)
        if x1 <= x0 or y1 <= y0:
            return Image(0, 0, b"")
        rows = [self.rgb[(y * self.width + x0) * 3:(y * self.width + x1) * 3] for y in range(y0, y1)]
        return Image(x1 - x0, y1 - y0, b"".join(rows))


def _chunks(data: bytes):
    if data[:8] != SIGNATURE:
        raise PngError("not a PNG file")
    pos = 8
    while pos + 12 <= len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if len(body) != length:
            raise PngError("truncated chunk %r" % kind)
        crc, = struct.unpack(">I", data[pos + 8 + length:pos + 12 + length])
        if zlib.crc32(kind + body) & 0xFFFFFFFF != crc:
            raise PngError("CRC mismatch in chunk %r" % kind)
        yield kind, body
        pos += 12 + length
        if kind == b"IEND":
            return
    raise PngError("missing IEND")


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def _unfilter(raw: bytes, width: int, height: int, bpp: int) -> bytearray:
    stride = width * bpp
    if len(raw) != height * (stride + 1):
        raise PngError("image data has %d bytes, expected %d" % (len(raw), height * (stride + 1)))
    out = bytearray(height * stride)
    prev = bytearray(stride)
    for y in range(height):
        kind = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if kind == 0:
            pass
        elif kind == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif kind == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif kind == 3:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif kind == 4:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                upleft = prev[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + _paeth(left, prev[i], upleft)) & 0xFF
        else:
            raise PngError("unknown filter type %d in row %d" % (kind, y))
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return out


def decode(data: bytes) -> Image:
    header = None
    idat = []
    for kind, body in _chunks(data):
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat.append(body)
        elif kind == b"PLTE":
            raise PngError("palette images are not supported")
    if header is None:
        raise PngError("missing IHDR")
    width, height, depth, colour, compression, filtering, interlace = header
    if depth != 8 or colour not in _CHANNELS:
        raise PngError("unsupported format: bit depth %d, colour type %d" % (depth, colour))
    if compression != 0 or filtering != 0 or interlace != 0:
        raise PngError("unsupported compression/filter/interlace %d/%d/%d" % (compression, filtering, interlace))
    if width == 0 or height == 0:
        raise PngError("empty image")
    bpp = _CHANNELS[colour]
    try:
        raw = zlib.decompress(b"".join(idat))
    except zlib.error as exc:
        raise PngError("corrupt image data: %s" % exc) from None
    pixels = _unfilter(raw, width, height, bpp)
    if colour == 2:
        rgb = bytes(pixels)
    elif colour == 6:
        rgb = bytes(b for i, b in enumerate(pixels) if i % 4 != 3)
    else:
        rgb = bytes(v for v in pixels for _ in range(3))
    return Image(width, height, rgb)


def read(path) -> Image:
    return decode(Path(path).read_bytes())


def encode(image: Image) -> bytes:
    stride = image.width * 3
    raw = b"".join(b"\x00" + image.rgb[y * stride:(y + 1) * stride] for y in range(image.height))

    def chunk(kind: bytes, body: bytes) -> bytes:
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    return (SIGNATURE + chunk(b"IHDR", struct.pack(">IIBBBBB", image.width, image.height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def write(path, image: Image) -> None:
    Path(path).write_bytes(encode(image))
