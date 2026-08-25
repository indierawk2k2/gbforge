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
GENERATOR_OWNED = ("ui_tile_data", "title_logo_tiles", "ghost_tile_data",
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
