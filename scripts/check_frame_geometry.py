#!/usr/bin/env python3
"""Frame-geometry check: no stray line segments in game output.

Given a screenshot and a frame rectangle, verifies the border renders
as a clean rectangle:

  1. every edge is a continuous line between its corners (small
     notches at the corner points are tolerated — that's the house
     corner style), and
  2. no line-colored pixels OVERHANG past the corners along any edge
     axis — the "crossing lines" defect class, where a corner tile's
     arm extends beyond the joint.

Intentional overlaps (a portrait box breaking a dialogue border, a
counter row crossing the frame) are declared, not silently accepted:
pass --allow x0,y0,x1,y1 for each region to exempt.

Usage:
  check_frame_geometry.py <png> --rect x0,y0,x1,y1 [--allow ...] ...

The rect is the frame's line rectangle in pixels (inclusive). Exits
nonzero with a per-defect report on failure.
"""

import struct
import sys
import zlib


def load_png(path):
    """Minimal PNG reader for the RGB/RGBA non-interlaced files the
    tooling here writes. Returns (width, height, pixel_fn)."""
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos = 8
    w = h = None
    bpp = 3
    idat = b""
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        tag = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        if tag == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", body[:10])
            assert depth == 8 and ctype in (2, 6), "unsupported PNG"
            bpp = 3 if ctype == 2 else 4
        elif tag == b"IDAT":
            idat += body
        pos += 12 + length
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows = []
    prev = bytearray(stride)
    p = 0
    for _ in range(h):
        filt = raw[p]
        line = bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        if filt == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif filt == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif filt == 3:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif filt == 4:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        rows.append(bytes(line))
        prev = line

    def pixel(x, y):
        o = x * bpp
        return rows[y][o:o + 3]

    return w, h, pixel


def parse_rect(s):
    v = [int(t) for t in s.split(",")]
    assert len(v) == 4
    return tuple(v)


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    path = args[0]
    rect = None
    allows = []
    i = 1
    while i < len(args):
        if args[i] == "--rect":
            rect = parse_rect(args[i + 1]); i += 2
        elif args[i] == "--allow":
            allows.append(parse_rect(args[i + 1])); i += 2
        else:
            print(f"unknown arg {args[i]}"); return 2
    assert rect, "--rect required"
    x0, y0, x1, y1 = rect

    w, h, pixel = load_png(path)
    # the line color is whatever the frame's top edge is drawn in,
    # sampled mid-edge
    line_rgb = pixel((x0 + x1) // 2, y0)

    def is_line(x, y):
        if not (0 <= x < w and 0 <= y < h):
            return False
        for ax0, ay0, ax1, ay1 in allows:
            if ax0 <= x <= ax1 and ay0 <= y <= ay1:
                return False
        p = pixel(x, y)
        return all(abs(p[i] - line_rgb[i]) <= 8 for i in range(3))

    NOTCH = 3   # tolerated corner-notch gap, pixels
    errors = []

    # 1) edge continuity (excluding the notch zone at each corner)
    for y, name in ((y0, "top"), (y1, "bottom")):
        gaps = [x for x in range(x0 + NOTCH, x1 - NOTCH + 1)
                if not is_line(x, y)]
        if gaps:
            errors.append(f"{name} edge broken at x={gaps[:6]}")
    for x, name in ((x0, "left"), (x1, "right")):
        gaps = [y for y in range(y0 + NOTCH, y1 - NOTCH + 1)
                if not is_line(x, y)]
        if gaps:
            errors.append(f"{name} edge broken at y={gaps[:6]}")

    # 2) no overhang past the corners along any edge axis
    OVER = 6    # how far past the corner to scan
    for y, name in ((y0, "top"), (y1, "bottom")):
        for x in range(max(0, x0 - OVER), x0):
            if is_line(x, y):
                errors.append(f"{name} edge overhangs left at ({x},{y})")
                break
        for x in range(x1 + 1, min(w, x1 + 1 + OVER)):
            if is_line(x, y):
                errors.append(f"{name} edge overhangs right at ({x},{y})")
                break
    for x, name in ((x0, "left"), (x1, "right")):
        for y in range(max(0, y0 - OVER), y0):
            if is_line(x, y):
                errors.append(f"{name} edge overhangs top at ({x},{y})")
                break
        for y in range(y1 + 1, min(h, y1 + 1 + OVER)):
            if is_line(x, y):
                errors.append(f"{name} edge overhangs bottom at ({x},{y})")
                break

    if errors:
        print(f"check_frame_geometry: {path}: FAIL")
        for e in errors:
            print(f"  {e}")
        return 1
    print(f"check_frame_geometry: {path}: ok "
          f"(rect {rect}, {len(allows)} allowed overlap(s))")
    return 0


if __name__ == "__main__":
    sys.exit(main())
