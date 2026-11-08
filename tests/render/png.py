#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Converts a raw frame (one byte per pixel, 0 or 1) into an 8-bit greyscale PNG.

The pixel value given by --lit-bit becomes white, the other black, and every pixel is repeated
--scale times in both directions. Only the standard library is used (zlib and struct).
"""

import argparse
import struct
import sys
import zlib


def chunk(tag, data):
    body = tag + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, required=True)
    parser.add_argument("--height", type=int, required=True)
    parser.add_argument("--scale", type=int, required=True)
    parser.add_argument("--lit-bit", type=int, choices=(0, 1), required=True)
    parser.add_argument("raw")
    parser.add_argument("png")
    args = parser.parse_args()

    with open(args.raw, "rb") as f:
        data = f.read()
    if len(data) != args.width * args.height:
        sys.exit("%s: %d bytes, expected %d" % (args.raw, len(data), args.width * args.height))

    lut = bytes(255 if value == args.lit_bit else 0 for value in range(256))
    scale = args.scale
    lines = []
    for y in range(args.height):
        row = data[y * args.width:(y + 1) * args.width].translate(lut)
        scaled = b"".join(row[x:x + 1] * scale for x in range(args.width))
        lines.extend([b"\x00" + scaled] * scale)

    header = struct.pack(">IIBBBBB", args.width * scale, args.height * scale, 8, 0, 0, 0, 0)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", header)
        + chunk(b"IDAT", zlib.compress(b"".join(lines), 9))
        + chunk(b"IEND", b"")
    )
    with open(args.png, "wb") as f:
        f.write(png)


if __name__ == "__main__":
    main()
