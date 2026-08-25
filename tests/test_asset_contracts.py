"""The editor/runtime asset boundary, checked in CI.

The art tools regex-parse and regenerate res/*.c. A format drift
normally surfaces the next time a human opens the editor and finds
their work mangled — long after the change that caused it, and in the
one place automated testing usually can't reach. These checks fail the
build instead.

    python3 -m pytest tests/test_asset_contracts.py
"""

import os
import re
import sys

import pytest

_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, _REPO)

RES = os.path.join(_REPO, "examples", "cascadia", "res")
EDITOR = os.path.join(_REPO, "tools", "sprite-editor",
                      "Sources", "GBSpriteEditor")


def _res(name):
    return open(os.path.join(RES, name)).read()


def _editor(rel):
    return open(os.path.join(EDITOR, rel)).read()


def test_manifest_parses():
    from gbforge.model import assets
    m = assets.load(RES)
    assert m.palette("silver") == 7
    assert m.palette_names[0] == "Fire"
    missing = assets.REQUIRED_ARRAYS - m.arrays
    assert not missing, f"res no longer exports {sorted(missing)}"


def test_arrays_are_unsized():
    """The editor's importer greps for `name[]`. A sized declaration
    compiles identically and breaks the tool silently."""
    from gbforge.model import assets
    m = assets.load(RES)
    for name in assets.REQUIRED_ARRAYS:
        assert name in m.arrays, f"{name} is not exported unsized"


def test_palettes_use_the_rgb_macro():
    """The importer extracts RGB(r,g,b) triples and the '// Name (N)'
    slot comments. Raw hex parses as zero palettes — the editor opens
    on an empty document and the artist's next save wipes the file."""
    src = _res("palettes.c")
    rgbs = re.findall(r"RGB\s*\(\s*\d+\s*,\s*\d+\s*,\s*\d+\s*\)", src)
    assert len(rgbs) == 32, f"expected 8 bg palettes x 4, got {len(rgbs)}"
    assert re.search(r"//\s*Fire\s*\(0\)", src), "slot comments are missing"


# What the editor actually round-trips, versus what the generator
# owns. Getting this split wrong is how an export deletes a symbol the
# runtime links against — see res/sprites_data.c, which holds
# sprite_palettes precisely because palettes.c is the editor's.
EDITOR_ROUNDTRIPPED = ("tile_data", "spell_icon_tile_data",
                       "bg_palettes", "tile_palette_map")
GENERATOR_OWNED = ("ui_tile_data", "ghost_tile_data",
                   "sprite_palettes")


def test_importer_and_exporter_agree_on_array_names():
    """Both halves of the editor name the same symbols. Catches a
    rename applied to one side only — which the round-trip script
    cannot catch, because it exercises the pair together and a
    consistently-wrong pair round-trips fine."""
    imp = _editor("Services/CImporter.swift")
    exp = _editor("Services/CExporter.swift")
    for name in EDITOR_ROUNDTRIPPED:
        assert name in imp, f"importer no longer mentions {name}"
        assert name in exp, f"exporter no longer mentions {name}"


def test_generator_owned_symbols_are_not_in_editor_owned_files():
    """A symbol the editor does not round-trip must not live in a file
    the editor rewrites, or the next export deletes it.

    This is not hypothetical: sprite_palettes started in palettes.c,
    and the first real editor export over res/ produced a ROM that
    failed to link with 'Undefined Global _sprite_palettes'.
    """
    editor_owned = ("tiles_data.c", "palettes.c", "spell_icons.c")
    for fn in editor_owned:
        src = _res(fn)
        for name in GENERATOR_OWNED:
            assert not re.search(rf"\b{name}\s*\[", src), (
                f"{name} lives in editor-owned {fn}; an export from the "
                f"art tool would delete it")


def test_vwf_pool_clears_every_static_tile():
    """Overlay text is uploaded at VWF_POOL_BASE. If the pool starts
    below a tile the game keeps loaded, showing an overlay corrupts
    that graphic — and only in the frames the overlay is up."""
    from gbforge.model import assets
    m = assets.load(RES)
    base, size = m.vwf_pool
    assert base and size, "res/merlin_font.h defines no VWF pool"
    assert base + size <= 256, "pool runs past the 256-tile index space"

    h = _res("tiles_data.h")
    ui_count = int(re.search(r"#define UI_TILE_COUNT (\d+)", h).group(1))
    icons = int(re.search(r"#define SPELL_ICON_COUNT (\d+)", h).group(1))
    game_tiles = int(re.search(r"#define TILE_TYPE_COUNT (\d+)", h).group(1)) * 4
    assert base >= game_tiles + ui_count + icons, (
        f"VWF pool at {base} overlaps statically loaded tiles "
        f"(they end at {game_tiles + ui_count + icons})")


@pytest.mark.skipif(not os.path.exists("/usr/bin/swift")
                    and not os.environ.get("SWIFT"),
                    reason="swiftc not available")
def test_editor_round_trip_is_byte_stable():
    """import -> export -> import -> export must be byte-identical.

    Slow (it compiles the editor's model layer), so it lives behind
    the same script CI runs: scripts/editor-roundtrip.sh.
    """
    import subprocess
    script = os.path.join(_REPO, "scripts", "editor-roundtrip.sh")
    r = subprocess.run([script, RES], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "ok" in r.stdout


def _ui_tile(index):
    """One 8x8 tile out of ui_tile_data[], as 8 rows of ink bitmasks."""
    src = _res("ui_tiles.c")
    m = re.search(r"const\s+uint8_t\s+ui_tile_data\s*\[\s*\]\s*=\s*{(.*?)}\s*;",
                  src, re.S)
    body = re.sub(r"//[^\n]*|/\*.*?\*/", "", m.group(1), flags=re.S)
    data = [int(t, 0) for t in re.findall(r"0[xX][0-9a-fA-F]+|\b\d+\b", body)]
    tile = data[index * 16:(index + 1) * 16]
    return [tile[r * 2] | tile[r * 2 + 1] for r in range(8)]


def _base(name):
    h = _res("tiles_data.h")
    m = re.search(rf"#define\s+{name}\s+\(UI_TILE_BASE \+ (\d+)\)", h)
    assert m, f"{name} is not defined"
    return int(m.group(1))


@pytest.mark.parametrize("ov,lo,dy", [
    ("UI_TILE_DIGIT_OV1_0", "UI_TILE_DIGIT_S1_0", 3),
    ("UI_TILE_DIGIT_OV2_0", "UI_TILE_DIGIT_S2_0", 6),
])
def test_subtile_digit_sets_are_actually_shifted(ov, lo, dy):
    """The OVn/Sn digit sets must be a real pre-shifted pair, not
    aliases of the base set.

    A HUD row that sits dy pixels below its tile boundary draws OVn on
    the row above and Sn on its own. If the two sets are copies of the
    base glyphs, the text renders tile-aligned and the sub-tile offset
    silently does nothing — it looks almost right, which is worse than
    looking broken.
    """
    plain = _ui_tile(_base("UI_TILE_DIGIT_0") + 3)      # the glyph "3"
    upper = _ui_tile(_base(ov) + 3)
    lower = _ui_tile(_base(lo) + 3)

    ink = [r for r, v in enumerate(plain) if v]
    assert ink, "the base digit set has no ink"

    # every base row appears exactly dy rows lower, split across the pair
    for r in ink:
        target = r + dy
        got = upper[target] if target < 8 else lower[target - 8]
        assert got == plain[r], (
            f"{ov}/{lo}: base row {r} should appear at offset "
            f"{target} of the shifted pair")

    assert any(lower), f"{lo} is empty — the pair never straddles the boundary"
    assert upper != plain, f"{ov} is an unshifted copy of the base set"


def _title_table(name):
    src = _res("merlin_font.c")
    m = re.search(rf"{name}\[\d*\]\s*=\s*{{(.*?)}};", src, re.S)
    assert m, f"{name} missing from the res font"
    body = re.sub(r"//[^\n]*|/\*.*?\*/", "", m.group(1), flags=re.S)
    return [int(t, 0) for t in
            re.findall(r"0[xX][0-9a-fA-F]+|\b\d+\b", body)]


def test_display_face_exists_and_is_16px():
    """The title logo is baked in a second, 16x16 weight — 32 bytes a
    glyph, two per row. Without it gen_title falls back to nothing and
    the build fails, which is the intended outcome; this says so."""
    widths = _title_table("title_widths")
    bitmaps = _title_table("title_bitmaps")
    assert widths, "no title_widths in the res font"
    assert len(bitmaps) == len(widths) * 32, (
        f"{len(bitmaps)} bitmap bytes for {len(widths)} glyphs — "
        f"expected 32 each (16 rows x 2 bytes)")


def test_display_face_is_proportional():
    """Advances must differ per letter.

    A monospaced display face is what made the old logo look like a
    placeholder: every letter got the same cell, so I floated in a
    hole and the run could only ever be centred to 8px. This is
    checked against the advance table rather than against a
    screenshot, because a rendered monospaced grid of narrow and wide
    letters still produces varying ink positions — the pixel test
    for this passes on the layout it is supposed to reject.
    """
    recode = _title_table("title_recode")
    widths = _title_table("title_widths")
    letters = {c: widths[recode[ord(c)]] for c in "IWAMLT"}
    assert len(set(letters.values())) > 1, (
        f"every letter advances the same: {letters}")
    assert letters["I"] < letters["W"], (
        f"I advances {letters['I']}px and W {letters['W']}px — "
        f"the face is not proportional")


def _title_glyph_rows(gi, bitmaps):
    """16 rows of ink bitmasks for display-face glyph `gi`."""
    return [(bitmaps[gi * 32 + r * 2] << 8) | bitmaps[gi * 32 + r * 2 + 1]
            for r in range(16)]


def test_display_face_has_one_cap_height():
    """Every letter must sit on the same baseline and reach the same
    cap line.

    A single glyph one row taller than the rest is not obviously wrong
    in isolation — it looks like a slightly bolder letter — but in a
    word it reads as a typo. The 16px A shipped a row taller than the
    other 35 glyphs and had to be recut; this is the check that would
    have caught it at build time.
    """
    recode = _title_table("title_recode")
    widths = _title_table("title_widths")
    bitmaps = _title_table("title_bitmaps")

    extents = {}
    for ch in "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789":
        gi = recode[ord(ch)]
        rows = _title_glyph_rows(gi, bitmaps)
        inked = [i for i, v in enumerate(rows) if v]
        assert inked, f"display-face glyph {ch!r} is blank"
        extents.setdefault((min(inked), max(inked)), []).append(ch)

    assert len(extents) == 1, (
        "display face has mixed cap extents: "
        + "; ".join(f"rows {t}..{b} -> {''.join(cs)}"
                    for (t, b), cs in sorted(extents.items())))
    assert len(widths) * 32 == len(bitmaps)


# Letters whose strokes are all uprights, bowls, or shallow curves —
# every horizontal cut through their middle crosses full-weight
# strokes. Excluded, with reasons:
#
#   D          the bowl's right side is 2px by design, lighter than
#              the stem the way a drawn bowl is
#   M W Q      2px where two diagonals meet at a vertex, and Q's tail
#   V X 0 4    diagonals, which are optically lighter at this size
#
# Those are drawing decisions. An upright that is simply a pixel too
# thin is not, which is what this separates.
STRAIGHT_SIDED = "ABCEFGHIJKLNOPRSTUYZ12356789"


def _mid_cap_runs(rows):
    """Horizontal ink run lengths across the middle of the cap."""
    inked = [i for i, v in enumerate(rows) if v]
    top, bot = min(inked), max(inked)
    out = set()
    for r in range(top + 5, min(top + 10, bot + 1)):
        n = 0
        for x in range(16):
            if rows[r] & (1 << (15 - x)):
                n += 1
            elif n:
                out.add(n)
                n = 0
        if n:
            out.add(n)
    return out


def test_display_face_stems_carry_the_face_weight():
    """Upright strokes must be the face's full 3px weight.

    The 16px I shipped with a 2px stem in an otherwise 3px face and
    read as a lighter letter inside a word; T and Y had the same
    defect where no logo had yet shown them. This makes that a build
    failure instead of something to catch on a real screen.
    """
    recode = _title_table("title_recode")
    bitmaps = _title_table("title_bitmaps")

    thin = {}
    for ch in STRAIGHT_SIDED:
        runs = _mid_cap_runs(_title_glyph_rows(recode[ord(ch)], bitmaps))
        if runs and min(runs) < 3:
            thin[ch] = sorted(runs)
    assert not thin, (
        "display-face glyphs with an under-weight upright: "
        + ", ".join(f"{c} {w}" for c, w in sorted(thin.items())))
