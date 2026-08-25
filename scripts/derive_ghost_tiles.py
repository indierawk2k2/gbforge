#!/usr/bin/env python3
"""Derive res/ghost_tiles.c from whatever the art tools last exported.

Ghost tiles are the faded copies the runtime draws for a tile in
transit during a refill. They are a mechanical darkening of the real
gem art, so asking an artist to draw and maintain a second set of
every tile is asking them to keep two things in sync by hand — which
is a bug generator, not a workflow.

Instead this runs on every build: read tile_data[] out of the
editor-owned tiles_data.c, map each pixel down one shade, write the
result. Redraw a gem in the editor, rebuild, and its ghost matches.

    python3 scripts/derive_ghost_tiles.py <res_dir>
"""

import os
import re
import sys

# 2bpp shade -> shade. Outline (3) becomes fill, fill (2) becomes
# shine, shine (1) stays: enough contrast to read as the same tile,
# little enough to read as "not really there yet".
FADE = {0: 0, 1: 1, 2: 1, 3: 2}


def parse_array(src, name):
    m = re.search(rf"const\s+uint8_t\s+{name}\s*\[\s*\]\s*=\s*{{(.*?)}}\s*;",
                  src, re.S)
    if not m:
        raise SystemExit(f"derive_ghost_tiles: no {name}[] in the res pack")
    body = re.sub(r"//[^\n]*|/\*.*?\*/", "", m.group(1), flags=re.S)
    return [int(t, 0) for t in re.findall(r"0[xX][0-9a-fA-F]+|\b\d+\b", body)]


def fade_tile(lo_hi):
    """One 16-byte 2bpp tile -> its faded copy."""
    out = bytearray(16)
    for r in range(8):
        lo, hi = lo_hi[r * 2], lo_hi[r * 2 + 1]
        nlo = nhi = 0
        for x in range(8):
            bit = 0x80 >> x
            v = (1 if lo & bit else 0) | (2 if hi & bit else 0)
            f = FADE[v]
            if f & 1:
                nlo |= bit
            if f & 2:
                nhi |= bit
        out[r * 2], out[r * 2 + 1] = nlo, nhi
    return out


def main(argv):
    res = argv[0] if argv else "res"
    src = open(os.path.join(res, "tiles_data.c")).read()
    data = parse_array(src, "tile_data")
    if len(data) % 16:
        raise SystemExit(
            f"derive_ghost_tiles: tile_data is {len(data)} bytes, "
            f"not a whole number of 16-byte tiles")

    faded = bytearray()
    for t in range(len(data) // 16):
        faded += fade_tile(data[t * 16:(t + 1) * 16])

    lines = []
    for i in range(0, len(faded), 12):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in faded[i:i + 12])
                     + ",")

    path = os.path.join(res, "ghost_tiles.c")
    with open(path, "w") as f:
        f.write("/* DERIVED from tiles_data.c by "
                "scripts/derive_ghost_tiles.py — do not edit.\n"
                " * Regenerated on every build; edit the gems in the "
                "sprite editor. */\n\n")
        f.write('#include <stdint.h>\n#include "tiles_data.h"\n\n')
        f.write(f"/* {len(faded) // 16} tiles, {len(faded)} bytes */\n")
        f.write("const uint8_t ghost_tile_data[] = {\n")
        f.write("\n".join(lines))
        f.write("\n};\n")
    print(f"derive_ghost_tiles: {len(faded) // 16} tiles -> {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
