"""Editor-owned asset boundary — gbforge READS res/, never writes it.

The art tools export res/*.{c,h} and own their contents. This module
parses them into a name to id manifest so a game definition can
reference assets symbolically — Tile("ruby"), Palette("silver") — and
codegen resolves against whatever the editors last exported.

That direction matters. If the generator owned the art files, every
re-export would be a merge conflict between a tool and a build step,
and the artist would lose. Instead the export IS the source, and
gbforge's job is to fail loudly when it stops matching what the
runtime compiles against.

The PINNED CONTRACT — array names, `#define` prefixes, the RGB()
palette format — is what the editors' exporters emit and their
importers re-parse. tests/test_asset_contracts.py fails the moment
either side drifts.
"""

import os
import re
from dataclasses import dataclass, field


def _strip_comments(src):
    return re.sub(r"//[^\n]*|/\*.*?\*/", "", src, flags=re.S)


def _defines(path, prefix):
    """{name: int} for '#define <PREFIX>NAME value' lines."""
    out = {}
    for m in re.finditer(rf"#define\s+({prefix}\w+)\s+(\d+)",
                         open(path).read()):
        out[m.group(1)] = int(m.group(2))
    return out


def _arrays(path):
    """Names of the top-level `const uint8_t NAME[] = {` arrays.

    Unsized by contract: the editors' importers grep for `name[]`, so
    a sized declaration is a different symbol to them even though the
    C compiler cannot tell the difference.
    """
    src = _strip_comments(open(path).read())
    return set(re.findall(r"const\s+uint\d+_t\s+(\w+)\s*\[\s*\]", src))


def _palette_names(path):
    """{index: name} from the '// Name (N)' comments in palettes.c.

    The index inside the parens is authoritative, never the position:
    a slot may be renamed or annotated without moving, and a comment
    may be missing entirely.
    """
    out = {}
    for m in re.finditer(r"//\s*([A-Za-z][\w ]*?)\s*\((\d+)\b", open(path).read()):
        out[int(m.group(2))] = m.group(1).strip()
    return out


@dataclass
class ResManifest:
    res_dir: str
    tiles: dict = field(default_factory=dict)      # TILE_* -> id
    palettes: dict = field(default_factory=dict)   # PAL_*  -> slot
    palette_names: dict = field(default_factory=dict)  # slot -> "Fire"
    arrays: set = field(default_factory=set)       # exported array names
    vwf_pool: tuple = (0, 0)                       # (base, size)

    def tile(self, name):
        return self.tiles[f"TILE_{name.upper()}"]

    def palette(self, name):
        return self.palettes[f"PAL_{name.upper()}"]


# What the runtime compiles against and the editors export. A name
# leaving this set is a breaking change to both at once.
#
# The title logo is deliberately absent: it is baked by gen_title from
# the spec's text in the display face, so res owns its tile RANGE
# (title_logo.h) but not its pixels.
REQUIRED_ARRAYS = {
    "tile_data",              # 16x16 board cells, 4 hw tiles each
    "ui_tile_data",           # digits, letters, borders, corners
    "spell_icon_tile_data",   # ability icons
    "ghost_tile_data",        # faded refill transit variants
    "cursor_sprite_data",
}


def load(res_dir):
    """Parse the res tree into a manifest. Raises if a contract file
    is missing — a broken editor boundary must fail loudly, not
    degrade into a game that renders the wrong tiles."""
    m = ResManifest(res_dir=os.path.abspath(res_dir))
    tiles_h = os.path.join(res_dir, "tiles_data.h")
    m.tiles = _defines(tiles_h, "TILE_")
    m.tiles.update(_defines(tiles_h, "UI_TILE_"))
    m.palettes = _defines(os.path.join(res_dir, "palettes.h"), "PAL_")
    m.palette_names = _palette_names(os.path.join(res_dir, "palettes.c"))
    # Scan every .c in the pack rather than a fixed list: which FILE
    # a symbol lives in is an ownership decision that moves (see
    # sprite_palettes), and the manifest should only care that the
    # contract's symbols exist somewhere.
    m.arrays = set()
    for fn in sorted(os.listdir(res_dir)):
        if fn.endswith(".c"):
            m.arrays |= _arrays(os.path.join(res_dir, fn))
    font_h = os.path.join(res_dir, "merlin_font.h")
    if os.path.exists(font_h):
        d = _defines(font_h, "VWF_POOL_")
        m.vwf_pool = (d.get("VWF_POOL_BASE", 0), d.get("VWF_POOL_SIZE", 0))
    return m
