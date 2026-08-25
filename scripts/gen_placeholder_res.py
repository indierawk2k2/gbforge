#!/usr/bin/env python3
"""Generate the placeholder asset pack (res/*.{c,h}) for an example game.

The gbforge runtime compiles against a fixed asset contract: tile and
sprite data arrays plus the index constants that name them (see the
emitted headers). Real games export these files from art tooling; the
examples generate them procedurally so the repository ships zero
borrowed art. Everything here is deliberately placeholder: flat-color
gems with distinct shapes, a plain block font, one-pixel borders.

Usage: gen_placeholder_res.py <out_dir> [--logo TEXT]

Also writes <out_dir>/../art/<gem>.png previews (pure-python PNG).
"""

import os
import struct
import sys
import zlib

# ── 2bpp helpers ─────────────────────────────────────────────────────

def tile_2bpp(rows):
    """8 rows of 8 pixel values (0-3) -> 16 GB 2bpp bytes."""
    out = []
    for row in rows:
        lo = hi = 0
        for x, px in enumerate(row):
            if px & 1:
                lo |= 0x80 >> x
            if px & 2:
                hi |= 0x80 >> x
        out += [lo, hi]
    return out


def blank(color=0):
    return [[color] * 8 for _ in range(8)]


# ── shapes: each gem is a 16x16 cell = 4 hardware tiles ─────────────

def gem_cell(shape):
    """16x16 pixel grid: color 3 outline, color 2 fill, color 1 shine,
    on color 0 background. Shapes are intentionally simple."""
    g = [[0] * 16 for _ in range(16)]

    def inside(x, y):
        cx, cy = x - 7.5, y - 7.5
        if shape == "diamond":
            return abs(cx) + abs(cy) <= 6.5
        if shape == "square":
            return abs(cx) <= 5 and abs(cy) <= 5
        if shape == "circle":
            return cx * cx + cy * cy <= 42
        if shape == "triangle":
            return y >= 3 and abs(cx) <= (y - 2) * 0.55 and y <= 13
        raise ValueError(shape)

    for y in range(16):
        for x in range(16):
            if inside(x, y):
                g[y][x] = 2
    # outline: filled pixel with a non-filled 4-neighbour
    for y in range(16):
        for x in range(16):
            if g[y][x] == 2:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if not (0 <= nx < 16 and 0 <= ny < 16) or g[ny][nx] == 0:
                        g[y][x] = 3
                        break
    # shine dot
    for y in (5, 6):
        for x in (5, 6):
            if g[y][x] == 2:
                g[y][x] = 1
    return g


def cell_tiles(g):
    """16x16 grid -> 4 tiles in quadrant order TL, TR, BL, BR."""
    quads = []
    for qy in (0, 8):
        for qx in (0, 8):
            quads.append([[g[qy + y][qx + x] for x in range(8)]
                          for y in range(8)])
    return quads


# ── block font: 8x8 caps, digits, punctuation (original glyphs) ─────

FONT = {
    "A": ["01111100", "11000110", "11000110", "11111110",
          "11000110", "11000110", "11000110", "00000000"],
    "B": ["11111100", "11000110", "11111100", "11000110",
          "11000110", "11000110", "11111100", "00000000"],
    "C": ["01111100", "11000110", "11000000", "11000000",
          "11000000", "11000110", "01111100", "00000000"],
    "D": ["11111000", "11001100", "11000110", "11000110",
          "11000110", "11001100", "11111000", "00000000"],
    "E": ["11111110", "11000000", "11111100", "11000000",
          "11000000", "11000000", "11111110", "00000000"],
    "F": ["11111110", "11000000", "11111100", "11000000",
          "11000000", "11000000", "11000000", "00000000"],
    "G": ["01111100", "11000110", "11000000", "11011110",
          "11000110", "11000110", "01111100", "00000000"],
    "H": ["11000110", "11000110", "11111110", "11000110",
          "11000110", "11000110", "11000110", "00000000"],
    "I": ["01111100", "00010000", "00010000", "00010000",
          "00010000", "00010000", "01111100", "00000000"],
    "J": ["00011110", "00000110", "00000110", "00000110",
          "11000110", "11000110", "01111100", "00000000"],
    "K": ["11000110", "11001100", "11111000", "11110000",
          "11011000", "11001100", "11000110", "00000000"],
    "L": ["11000000", "11000000", "11000000", "11000000",
          "11000000", "11000000", "11111110", "00000000"],
    "M": ["11000110", "11101110", "11111110", "11010110",
          "11000110", "11000110", "11000110", "00000000"],
    "N": ["11000110", "11100110", "11110110", "11011110",
          "11001110", "11000110", "11000110", "00000000"],
    "O": ["01111100", "11000110", "11000110", "11000110",
          "11000110", "11000110", "01111100", "00000000"],
    "P": ["11111100", "11000110", "11000110", "11111100",
          "11000000", "11000000", "11000000", "00000000"],
    "Q": ["01111100", "11000110", "11000110", "11000110",
          "11010110", "11001100", "01110110", "00000000"],
    "R": ["11111100", "11000110", "11000110", "11111100",
          "11011000", "11001100", "11000110", "00000000"],
    "S": ["01111100", "11000110", "11000000", "01111100",
          "00000110", "11000110", "01111100", "00000000"],
    "T": ["11111110", "00010000", "00010000", "00010000",
          "00010000", "00010000", "00010000", "00000000"],
    "U": ["11000110", "11000110", "11000110", "11000110",
          "11000110", "11000110", "01111100", "00000000"],
    "V": ["11000110", "11000110", "11000110", "11000110",
          "01101100", "00111000", "00010000", "00000000"],
    "W": ["11000110", "11000110", "11000110", "11010110",
          "11111110", "11101110", "11000110", "00000000"],
    "X": ["11000110", "01101100", "00111000", "00111000",
          "01101100", "11000110", "11000110", "00000000"],
    "Y": ["11000110", "11000110", "01101100", "00111000",
          "00010000", "00010000", "00010000", "00000000"],
    "Z": ["11111110", "00001100", "00011000", "00110000",
          "01100000", "11000000", "11111110", "00000000"],
    "0": ["01111100", "11000110", "11001110", "11010110",
          "11100110", "11000110", "01111100", "00000000"],
    "1": ["00011000", "00111000", "00011000", "00011000",
          "00011000", "00011000", "01111110", "00000000"],
    "2": ["01111100", "11000110", "00000110", "00111100",
          "01100000", "11000000", "11111110", "00000000"],
    "3": ["01111100", "11000110", "00000110", "00011100",
          "00000110", "11000110", "01111100", "00000000"],
    "4": ["00001100", "00011100", "00111100", "01101100",
          "11111110", "00001100", "00001100", "00000000"],
    "5": ["11111110", "11000000", "11111100", "00000110",
          "00000110", "11000110", "01111100", "00000000"],
    "6": ["01111100", "11000000", "11111100", "11000110",
          "11000110", "11000110", "01111100", "00000000"],
    "7": ["11111110", "00000110", "00001100", "00011000",
          "00110000", "00110000", "00110000", "00000000"],
    "8": ["01111100", "11000110", "11000110", "01111100",
          "11000110", "11000110", "01111100", "00000000"],
    "9": ["01111100", "11000110", "11000110", "01111110",
          "00000110", "00000110", "01111100", "00000000"],
    ":": ["00000000", "00011000", "00011000", "00000000",
          "00011000", "00011000", "00000000", "00000000"],
}


def glyph(ch, color=3):
    rows = FONT.get(ch)
    if rows is None:
        return blank()
    return [[color if bit == "1" else 0 for bit in row] for row in rows]


def vwf_font():
    """VWF font tables baked from FONT, in the res contract's shape.

    The runtime never composes text at run time — gbforge's gen_ui
    bakes each overlay line into ready-to-upload tiles at build time —
    so this is build-time data, not ROM data. It ships as C because
    the same tables in a real game ARE compiled in (for dialogue and
    any runtime-composed string), and the codegen parses one format.

    Returns (recode[128], widths[n], bitmaps[n*8], order) where a
    glyph's width is its ink extent plus one column of side bearing,
    which is what makes the font proportional: "1" is narrower than
    "W" and centering lands on real pixels rather than tile edges.
    """
    order = [" "] + sorted(FONT)
    recode = [0] * 128
    widths, bitmaps = [], []
    for idx, ch in enumerate(order):
        rows = FONT.get(ch, ["00000000"] * 8)
        bits = [int(r, 2) for r in rows]
        ink = 0
        for b in bits:
            for x in range(8):
                if b & (0x80 >> x):
                    ink = max(ink, x + 1)
        widths.append(min(8, ink + 1) if ink else 3)   # space = 3px
        bitmaps.append(bits)
        if ch != " ":
            recode[ord(ch)] = idx
            if ch.isalpha():
                recode[ord(ch.lower())] = idx          # fold lowercase
    return recode, widths, bitmaps, order


def emit_vwf_font(hdr, pool_base, pool_size):
    """(merlin_font.c, merlin_font.h) source text."""
    recode, widths, bitmaps, order = vwf_font()

    rows = []
    for i in range(0, 128, 16):
        rows.append("    " + ", ".join(f"{v:3d}" for v in recode[i:i + 16])
                    + f",  // 0x{i:02X}-0x{i + 15:02X}")
    recode_c = "\n".join(rows)

    width_c = "\n".join(
        "    " + ", ".join(str(w) for w in widths[i:i + 12]) + ","
        for i in range(0, len(widths), 12))

    glyph_c = "\n".join(
        "    " + ", ".join(f"0x{b:02X}" for b in bitmaps[i]) +
        f",  // {i}: {order[i] if order[i] != ' ' else 'SPC'}"
        for i in range(len(bitmaps)))

    c = (hdr + f"""
/* Proportional 1bpp font, MSB-left, 8 rows per glyph.
 *
 * BUILD-TIME DATA: gbforge's gen_ui parses these tables to bake
 * overlay text into tiles. Nothing links this file into the ROM,
 * which is why it needs no GBDK headers.
 */

#include <stdint.h>

const uint8_t merlin_recode[128] = {{
{recode_c}
}};

const uint8_t merlin_widths[{len(widths)}] = {{
{width_c}
}};

const uint8_t merlin_bitmaps[{len(bitmaps) * 8}] = {{
{glyph_c}
}};
""")

    h = (hdr + f"""#ifndef MERLIN_FONT_H
#define MERLIN_FONT_H

/* VWF tile pool — the runtime asset contract.
 *
 * ui_show_overlay() uploads an overlay's baked text spans starting at
 * VWF_POOL_BASE. The pool sits above every statically-loaded tile so
 * an overlay can never clobber board, UI, or icon graphics; it DOES
 * share the ghost/logo range, which is safe because the two never
 * appear on screen together (see tiles_data.h).
 */
#define VWF_POOL_BASE  {pool_base}
#define VWF_POOL_SIZE  {pool_size}

#endif
""")
    return c, h


def small_glyph(ch, color=3):
    """5x5-ish reduction of a glyph for the small-digit set."""
    g = glyph(ch, color)
    out = blank()
    for y in range(6):
        for x in range(6):
            out[y + 1][x + 1] = g[y + 1][x + 1]
    return out


# ── misc UI shapes ───────────────────────────────────────────────────

def solid(color):
    return blank(color)


def rounded(color):
    """7x7 rounded block: the last row and column stay clear so
    vertically stacked counters keep a 1px gap."""
    g = blank()
    for y in range(7):
        for x in range(7):
            g[y][x] = color
    for x, y in ((0, 0), (6, 0), (0, 6), (6, 6)):
        g[y][x] = 0
    return g


def hline(row, color=3):
    g = blank()
    g[row] = [color] * 8
    return g


def vline(col, color=3):
    g = blank()
    for y in range(8):
        g[y][col] = color
    return g


def corner_l(hrow, vcol, hdir, vdir, notch=True, color=3):
    """Corner tile: an L, not a cross. The horizontal arm runs from
    the elbow toward the adjacent border tile (hdir: +1 right,
    -1 left), the vertical arm likewise (vdir: +1 down, -1 up), and
    both STOP at the elbow — nothing overhangs the joint. The elbow
    pixel is notched (cleared) for the house corner style."""
    g = blank()
    xs = range(vcol, 8) if hdir > 0 else range(0, vcol + 1)
    for x in xs:
        g[hrow][x] = color
    ys = range(hrow, 8) if vdir > 0 else range(0, hrow + 1)
    for y in ys:
        g[y][vcol] = color
    if notch:
        g[hrow][vcol] = 0
    return g


def arrow_right(color=3):
    g = blank()
    for y in range(1, 7):
        w = y if y <= 3 else 7 - y
        for x in range(1, 1 + w * 2):
            if x < 8:
                g[y][x] = color
    return g


def tri(up, color=3):
    g = blank()
    for i in range(4):
        y = (1 + i) if up else (6 - i)
        for x in range(3 - i, 5 + i):
            g[y][x] = color
    return g


def bracket(left, color=3):
    g = blank()
    col = 1 if left else 6
    for y in range(1, 7):
        g[y][col] = color
    g[1][col + (1 if left else -1)] = color
    g[6][col + (1 if left else -1)] = color
    return g


def checkmark(color=3):
    g = blank()
    pts = [(1, 4), (2, 5), (3, 6), (4, 5), (5, 4), (6, 3), (7, 2)]
    for x, y in pts:
        if 0 <= y < 8:
            g[y][x] = color
    return g


def dot(color=3):
    g = blank()
    for y in (3, 4):
        for x in (3, 4):
            g[y][x] = color
    return g


def cursor_corner(qx, qy, invert):
    """L-bracket corner: 3px arms, 1px border, in a 8x8 quadrant."""
    fg, bg = (1, 3) if not invert else (3, 1)
    g = blank()
    xs = range(0, 4) if qx == 0 else range(4, 8)
    ys = range(0, 4) if qy == 0 else range(4, 8)
    for y in ys:
        for x in xs:
            edge_x = (x == (0 if qx == 0 else 7))
            edge_y = (y == (0 if qy == 0 else 7))
            arm_x = (x in (0, 1) if qx == 0 else x in (6, 7))
            arm_y = (y in (0, 1) if qy == 0 else y in (6, 7))
            if arm_x or arm_y:
                g[y][x] = bg if (edge_x or edge_y) else fg
    return g


def burst(frame):
    g = blank()
    if frame == 0:
        for y in (3, 4):
            for x in (3, 4):
                g[y][x] = 3
    elif frame == 1:
        for d in range(2, 6):
            g[d][d] = g[d][7 - d] = 3
    else:
        for i in range(1, 7):
            g[i][i] = g[i][7 - i] = g[3][i] = g[i][3] = 3
    return g


def plus(color=3):
    g = blank()
    for i in range(2, 6):
        g[i][3] = g[i][4] = g[3][i] = g[4][i] = color
    return g


# ── PNG preview writer (no dependencies) ────────────────────────────

def write_png(path, grid16, rgb):
    """grid16: 16x16 values 0-3; rgb: 4 (r,g,b) tuples."""
    raw = b""
    for row in grid16:
        raw += b"\x00" + b"".join(bytes(rgb[v]) for v in row)
    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(
            ">I", zlib.crc32(c) & 0xFFFFFFFF)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", 16, 16, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw))
           + chunk(b"IEND", b""))
    open(path, "wb").write(png)


# ── emit ─────────────────────────────────────────────────────────────

GEMS = [("ruby", "diamond"), ("emerald", "square"),
        ("topaz", "triangle"), ("onyx", "circle")]

# RGB555 palettes: bg 0-3 = gem colors on white; PAL 4-7 utility
PALETTES = [
    # (white, shine, fill, outline)
    ((31, 31, 31), (31, 22, 24), (26, 4, 8),  (10, 1, 2)),    # ruby
    ((31, 31, 31), (22, 31, 22), (4, 22, 8),  (1, 8, 2)),     # emerald
    ((31, 31, 31), (31, 30, 20), (28, 22, 4), (12, 9, 1)),    # topaz
    ((31, 31, 31), (20, 20, 24), (8, 8, 12),  (2, 2, 4)),     # onyx
    ((31, 31, 31), (24, 24, 24), (16, 16, 16), (0, 0, 0)),    # dark
    ((31, 31, 31), (28, 26, 8),  (22, 18, 4), (8, 6, 0)),     # yellow
    ((31, 31, 31), (30, 12, 12), (24, 4, 4),  (8, 0, 0)),     # red
    ((31, 31, 31), (24, 24, 26), (14, 14, 16), (0, 0, 0)),    # ui/silver
]


def rgb555(t):
    return t[0] | (t[1] << 5) | (t[2] << 10)


def rgb888(t):
    return tuple(v * 255 // 31 for v in t)


def fmt_bytes(vals, per=12):
    lines = []
    for i in range(0, len(vals), per):
        lines.append("    " + ", ".join(f"0x{v:02X}"
                                        for v in vals[i:i + per]) + ",")
    return "\n".join(lines)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "res"
    logo = "GBFORGE"
    if "--logo" in sys.argv:
        logo = sys.argv[sys.argv.index("--logo") + 1].upper()
    os.makedirs(out, exist_ok=True)
    art_dir = os.path.join(out, "..", "art")
    os.makedirs(art_dir, exist_ok=True)

    hdr = ("/* Auto-generated by scripts/gen_placeholder_res.py — do not\n"
           " * edit. Deliberately-placeholder assets satisfying the\n"
           " * gbforge runtime's asset contract. */\n")

    # game tiles: 12 logical types x 4 quadrants. 0 = empty; 1-4 gems;
    # 5-11 dimmed circles (unused by the example, present so any board
    # value the engine can hold renders as SOMETHING sane).
    game_tiles = cell_tiles([[0] * 16 for _ in range(16)])
    pal_map = [[7, 7, 7, 7]]
    for i, (name, shape) in enumerate(GEMS):
        game_tiles += cell_tiles(gem_cell(shape))
        pal_map.append([i, i, i, i])
        write_png(os.path.join(art_dir, f"{name}.png"), gem_cell(shape),
                  [rgb888(c) for c in PALETTES[i]])
    for _ in range(7):
        game_tiles += cell_tiles(gem_cell("circle"))
        pal_map.append([4, 4, 4, 4])

    # UI tiles, indices per the contract in tiles_data.h
    ui = [blank()] * 126
    for d in range(10):
        ui[0 + d] = glyph(str(d))
        ui[116 + d] = glyph(str(d))            # OV1 digit alias set
        ui[72 + d] = glyph(str(d))             # S1 set
        ui[82 + d] = glyph(str(d))             # S2 set
        ui[92 + d] = glyph(str(d))             # OV2 set
        ui[103 + d] = small_glyph(str(d))
    ui[10] = solid(2)
    ui[11] = blank()
    ui[12] = glyph("S")                        # score label
    ui[13] = blank()
    for i in range(26):
        ui[14 + i] = glyph(chr(ord("A") + i))
    ui[40] = arrow_right()
    ui[41] = checkmark()
    ui[42] = hline(7); ui[43] = hline(0)
    ui[44] = vline(7); ui[45] = vline(0)
    ui[46] = tri(True); ui[47] = tri(False)
    ui[48] = bracket(True); ui[49] = bracket(False)
    ui[50] = hline(6); ui[51] = hline(1)
    ui[52] = vline(6); ui[53] = vline(1)
    # frame corners join BORDER_TOP/BOT (rows 6/1) and LEFT/RIGHT
    # (cols 6/1); arms point into the frame's interior sides
    ui[54] = corner_l(6, 6, +1, +1)   # TL: arms right + down
    ui[55] = corner_l(6, 1, -1, +1)   # TR: arms left + down
    ui[56] = corner_l(1, 6, +1, -1)   # BL: arms right + up
    ui[57] = corner_l(1, 1, -1, -1)   # BR: arms left + up
    ui[58] = solid(3)
    ui[59] = rounded(2); ui[60] = rounded(3)
    # overlay corners join OVL borders (rows 7/0, cols 7/0) —
    # sharp elbows (no notch), arms stop at the joint
    ui[61] = corner_l(7, 7, +1, +1, notch=False)
    ui[62] = corner_l(7, 0, -1, +1, notch=False)
    ui[63] = corner_l(0, 7, +1, -1, notch=False)
    ui[64] = corner_l(0, 0, -1, -1, notch=False)
    ui[66] = tri(True, 1); ui[67] = tri(False, 1)
    ui[68] = rounded(2); ui[69] = rounded(2)
    ui[70] = rounded(2); ui[71] = rounded(2)
    ui[102] = dot()
    ui[115] = glyph(":")

    # spell/ability icons: 7 shapes + 7 dimmed copies
    icons = []
    for i in range(7):
        g = rounded(2)
        g[3][3] = g[3][4] = g[4][3] = g[4][4] = 3
        icons.append(g)
    icons += [rounded(1) for _ in range(7)]

    # title logo: block letters, 4 tiles per letter (16x16 scale-up)
    logo_tiles = []
    for ch in logo:
        g8 = glyph(ch)
        g16 = [[0] * 16 for _ in range(16)]
        for y in range(8):
            for x in range(8):
                v = g8[y][x]
                for dy in (0, 1):
                    for dx in (0, 1):
                        g16[y * 2 + dy][x * 2 + dx] = v
        logo_tiles += cell_tiles(g16)

    # sprite tiles 0-67 per sprites_data.h contract
    spr = [blank()] * 68
    spr[0] = cursor_corner(0, 0, False); spr[1] = cursor_corner(1, 0, False)
    spr[2] = cursor_corner(0, 1, False); spr[3] = cursor_corner(1, 1, False)
    spr[4] = cursor_corner(0, 0, True); spr[5] = cursor_corner(1, 0, True)
    spr[6] = cursor_corner(0, 1, True); spr[7] = cursor_corner(1, 1, True)
    spr[8] = glyph("S")                        # score label sprite
    for d in range(10):
        spr[9 + d] = glyph(str(d))
        spr[57 + d] = glyph(str(d))            # moves digits
    spr[19] = burst(0); spr[20] = burst(1); spr[21] = burst(2)
    spr[22] = plus()
    spr[55] = glyph("M"); spr[56] = glyph("V")
    spr[67] = glyph("C")

    def emit_tiles(name, tiles):
        data = []
        for t in tiles:
            data += tile_2bpp(t)
        return (f"const uint8_t {name}[{len(data)}] = {{\n"
                + fmt_bytes(data) + "\n};\n\n")

    with open(os.path.join(out, "tiles_data.c"), "w") as f:
        f.write(hdr + '#include <stdint.h>\n#include "tiles_data.h"\n#include "title_logo.h"\n\n')
        f.write(emit_tiles("tile_data", game_tiles))
        f.write(emit_tiles("ui_tile_data", ui))
        f.write(emit_tiles("spell_icon_tile_data", icons))
        f.write(emit_tiles("title_logo_tiles", logo_tiles))
        # ghost variants: refill drop-in transit tiles (fade map
        # 3->2, 2->1, matching the private pack's derivation)
        fade = {0: 0, 1: 1, 2: 1, 3: 2}
        ghosts = [[[fade[v] for v in row] for row in t]
                  for t in game_tiles]
        f.write(emit_tiles("ghost_tile_data", ghosts))
        f.write(f"const uint8_t tile_palette_map[][4] = {{\n")
        for row in pal_map:
            f.write("    { " + ", ".join(str(v) for v in row) + " },\n")
        f.write("};\n")

    with open(os.path.join(out, "sprites_data.c"), "w") as f:
        f.write(hdr + '#include <stdint.h>\n#include "sprites_data.h"\n\n')
        f.write(emit_tiles("cursor_sprite_data", spr[0:8]))
        f.write(emit_tiles("knowledge_sprite_data", spr[8:19]))
        f.write(emit_tiles("burst_sprite_data", spr[19:22]))
        f.write(emit_tiles("float_sprite_data", spr[22:23]))
        f.write(emit_tiles("chain_sprite_data", spr[23:51]))
        f.write(emit_tiles("hint_sprite_data", spr[51:55]))
        f.write(emit_tiles("moves_sprite_data", spr[55:67]))
        f.write(emit_tiles("opp_knowledge_sprite_data", spr[67:68]))

    with open(os.path.join(out, "palettes.c"), "w") as f:
        f.write(hdr + '#include <stdint.h>\n#include "palettes.h"\n\n')
        f.write("const uint16_t bg_palettes[32] = {\n")
        for p in PALETTES:
            f.write("    " + ", ".join(f"0x{rgb555(c):04X}"
                                       for c in p) + ",\n")
        f.write("};\n\nconst uint16_t sprite_palettes[32] = {\n")
        for p in PALETTES:
            f.write("    " + ", ".join(f"0x{rgb555(c):04X}"
                                       for c in p) + ",\n")
        f.write("};\n")

    n_logo = len(logo)
    # ghosts SHARE the logo's tile range: the logo lives only on the
    # title screen and ghosts only in gameplay — each screen's gfx
    # load overwrites the other's slice.
    ghost_base = 48 + len(ui) + len(icons)
    assert ghost_base + 48 <= 256, "ghost tiles overflow the tile index space"
    headers = {
        "tiles_data.h": f"""{hdr}#ifndef TILES_DATA_H
#define TILES_DATA_H

#include <stdint.h>

#define TILE_EMPTY 0
#define TILE_TYPE_COUNT 12
#define HW_TILES_PER_LOGICAL 4
#define HW_TILE_BASE(type) ((type) * HW_TILES_PER_LOGICAL)
#define TOTAL_GAME_HW_TILES (TILE_TYPE_COUNT * HW_TILES_PER_LOGICAL)

#define UI_TILE_BASE TOTAL_GAME_HW_TILES
#define UI_TILE_DIGIT_0      (UI_TILE_BASE + 0)
#define UI_TILE_SOLID        (UI_TILE_BASE + 10)
#define UI_TILE_BLANK        (UI_TILE_BASE + 11)
#define UI_TILE_LABEL_K      (UI_TILE_BASE + 12)
#define UI_TILE_LETTER_A     (UI_TILE_BASE + 14)
#define UI_TILE_ARROW        (UI_TILE_BASE + 40)
#define UI_TILE_CHECK        (UI_TILE_BASE + 41)
#define UI_TILE_OVL_BORDER_TOP   (UI_TILE_BASE + 42)
#define UI_TILE_OVL_BORDER_BOT   (UI_TILE_BASE + 43)
#define UI_TILE_OVL_BORDER_LEFT  (UI_TILE_BASE + 44)
#define UI_TILE_OVL_BORDER_RIGHT (UI_TILE_BASE + 45)
#define UI_TILE_TRI_UP       (UI_TILE_BASE + 46)
#define UI_TILE_TRI_DOWN     (UI_TILE_BASE + 47)
#define UI_TILE_SEL_L        (UI_TILE_BASE + 48)
#define UI_TILE_SEL_R        (UI_TILE_BASE + 49)
#define UI_TILE_BORDER_TOP   (UI_TILE_BASE + 50)
#define UI_TILE_BORDER_BOT   (UI_TILE_BASE + 51)
#define UI_TILE_BORDER_LEFT  (UI_TILE_BASE + 52)
#define UI_TILE_BORDER_RIGHT (UI_TILE_BASE + 53)
#define UI_TILE_CORNER_TL    (UI_TILE_BASE + 54)
#define UI_TILE_CORNER_TR    (UI_TILE_BASE + 55)
#define UI_TILE_CORNER_BL    (UI_TILE_BASE + 56)
#define UI_TILE_CORNER_BR    (UI_TILE_BASE + 57)
#define UI_TILE_SOLID3       (UI_TILE_BASE + 58)
#define UI_TILE_ROUND2       (UI_TILE_BASE + 59)

/* Ghost (faded) game-tile variants: refill drop-in transit tiles.
 * Shares the title logo's tile range ({ghost_base}+): the logo is
 * title-only and ghosts are game-only; each screen's gfx load
 * overwrites the other's slice. */
#define GHOST_TILE_BASE {ghost_base}
#define HW_TILE_BASE_GHOST(type) (GHOST_TILE_BASE + (type) * HW_TILES_PER_LOGICAL)
#define UI_TILE_ROUND3       (UI_TILE_BASE + 60)
#define UI_TILE_OVL_CORNER_TL (UI_TILE_BASE + 61)
#define UI_TILE_OVL_CORNER_TR (UI_TILE_BASE + 62)
#define UI_TILE_OVL_CORNER_BL (UI_TILE_BASE + 63)
#define UI_TILE_OVL_CORNER_BR (UI_TILE_BASE + 64)
#define UI_TILE_TRI_UP_DIS   (UI_TILE_BASE + 66)
#define UI_TILE_TRI_DOWN_DIS (UI_TILE_BASE + 67)
#define UI_TILE_ROUND_OV1    (UI_TILE_BASE + 68)
#define UI_TILE_ROUND_S1     (UI_TILE_BASE + 69)
#define UI_TILE_ROUND_OV2    (UI_TILE_BASE + 70)
#define UI_TILE_ROUND_S2     (UI_TILE_BASE + 71)
#define UI_TILE_DIGIT_S1_0   (UI_TILE_BASE + 72)
#define UI_TILE_DIGIT_S2_0   (UI_TILE_BASE + 82)
#define UI_TILE_DIGIT_OV2_0  (UI_TILE_BASE + 92)
#define UI_TILE_SMALL_DOT    (UI_TILE_BASE + 102)
#define UI_TILE_SMALL_DIGIT_0 (UI_TILE_BASE + 103)
#define UI_TILE_COLON        (UI_TILE_BASE + 115)
#define UI_TILE_DIGIT_OV1_0  (UI_TILE_BASE + 116)
#define UI_TILE_COUNT 126
#define TOTAL_HW_TILES (TOTAL_GAME_HW_TILES + UI_TILE_COUNT)

#define SPELL_ICON_COUNT 14
#define SPELL_ICON_TILE_BASE TOTAL_HW_TILES
#define UI_TILE_SPARK (SPELL_ICON_TILE_BASE + 0)

extern const uint8_t tile_data[];
extern const uint8_t ui_tile_data[];
extern const uint8_t spell_icon_tile_data[];
extern const uint8_t ghost_tile_data[];
extern const uint8_t tile_palette_map[][4];

#endif
""",
        "sprites_data.h": f"""{hdr}#ifndef SPRITES_DATA_H
#define SPRITES_DATA_H

#include <stdint.h>

#define CURSOR_SPRITE_BASE 0
#define CURSOR_TILE_NORMAL 0
#define CURSOR_TILE_INVERTED 4
#define CURSOR_SPRITE_COUNT 4
#define CURSOR_TOTAL_TILES 8

#define KNOWLEDGE_SPRITE_BASE 4
#define KNOWLEDGE_TILE_BASE 8
#define KNOWLEDGE_TILE_K 8
#define KNOWLEDGE_TILE_0 9
#define KNOWLEDGE_SPRITE_COUNT 5
#define KNOWLEDGE_TOTAL_TILES 11

#define BURST_SPRITE_BASE 9
#define BURST_SPRITE_COUNT 3
#define BURST_TILE_BASE 19
#define BURST_TILE_DOT 19
#define BURST_TILE_SMALL 20
#define BURST_TILE_FULL 21
#define BURST_TOTAL_TILES 3

#define FLOAT_SPRITE_BASE 12
#define FLOAT_TILE_PLUS 22
#define FLOAT_TOTAL_TILES 1
#define MAX_FLOATING_NUMS 4

#define CHAIN_SPRITE_BASE 20
#define CHAIN_DIGIT_TILE_TOP 23
#define CHAIN_DIGIT_TILE_BOT 30
#define CHAIN_WORD_TILE_TOP 37
#define CHAIN_WORD_TILE_BOT 44
#define CHAIN_WORD_COLS 7
#define CHAIN_TOTAL_TILES 28

#define HINT_SPRITE_BASE 36
#define GHOST_CURSOR_A_BASE 4    /* hint bracket A: aliases knowledge */
#define GHOST_CURSOR_B_BASE 36   /* hint bracket B: aliases hint/moves */
#define HINT_SPRITE_COUNT 4
#define HINT_TILE_BASE 51

#define MOVES_SPR_TILE_M 55
#define MOVES_SPR_TILE_V 56
#define MOVES_SPR_TILE_D0 57

#define OPP_KNOWLEDGE_SPRITE_BASE 20
#define OPP_KNOWLEDGE_SPRITE_COUNT 5
#define OPP_KNOWLEDGE_TILE_C 67
#define OPP_KNOWLEDGE_TOTAL_TILES 1

#define SWAP_TILE_A 68
#define SWAP_TILE_B 72
#define SWAP_SPRITE_A_BASE 25
#define SWAP_SPRITE_B_BASE 29
#define SWAP_PAL_A 4
#define SWAP_PAL_B 5

extern const uint8_t cursor_sprite_data[];
extern const uint8_t knowledge_sprite_data[];
extern const uint8_t burst_sprite_data[];
extern const uint8_t float_sprite_data[];
extern const uint8_t chain_sprite_data[];
extern const uint8_t hint_sprite_data[];
extern const uint8_t moves_sprite_data[];
extern const uint8_t opp_knowledge_sprite_data[];

#endif
""",
        "palettes.h": f"""{hdr}#ifndef PALETTES_H
#define PALETTES_H

#include <stdint.h>

#define PAL_FIRE 0
#define PAL_WATER 1
#define PAL_EARTH 2
#define PAL_BRONZE 3
#define PAL_DARK 4
#define PAL_GOLD 5
#define PAL_RUBY 6
#define PAL_SILVER 7
#define PAL_CURSOR 0
#define PAL_CURSOR_OPPONENT 4
#define PAL_GHOST_HINT 5
#define PAL_GHOST_UNSOLVABLE 6
#define PAL_A 0
#define PAL_B 1
#define PAL_RED 6
#define PAL_YELLOW 5
#define PAL_TARGETING 7

extern const uint16_t bg_palettes[32];
extern const uint16_t sprite_palettes[32];

#endif
""",
    }
    headers["title_logo.h"] = (hdr + f"""#ifndef TITLE_LOGO_H
#define TITLE_LOGO_H

#include <stdint.h>
#include "tiles_data.h"

#define TITLE_LOGO_TILE_BASE (SPELL_ICON_TILE_BASE + SPELL_ICON_COUNT)
#define TITLE_LOGO_TILE_COUNT ({n_logo} * 4)

extern const uint8_t title_logo_tiles[];

#endif
""")
    # VWF pool: above the title logo (title-only) and above the ghost
    # tiles any board type this example can hold (types 0-4 -> 188-207).
    # Overlay text and in-flight refill ghosts therefore never collide.
    pool_base = 48 + len(ui) + len(icons) + n_logo * 4
    pool_size = 256 - pool_base
    assert pool_base + pool_size <= 256, "VWF pool overflows the tile space"
    font_c, font_h = emit_vwf_font(hdr, pool_base, pool_size)
    open(os.path.join(out, "merlin_font.c"), "w").write(font_c)
    headers["merlin_font.h"] = font_h

    for name, text in headers.items():
        open(os.path.join(out, name), "w").write(text)

    n_tiles = len(game_tiles) + len(ui) + len(icons) + len(logo_tiles)
    print(f"gen_placeholder_res: {n_tiles} bg tiles, {len(spr)} sprite "
          f"tiles, 8 palettes -> {out}")


if __name__ == "__main__":
    main()
