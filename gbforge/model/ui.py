"""UI definitions — overlays with fully described chrome.

An Overlay carries everything about its appearance as data: which res
tiles form each side and corner, the palette, the font, screen
position/size, and its text lines. Codegen (gen_ui) resolves the
symbolic names to the C macros the editor-owned res headers define,
so re-skinning an overlay — or giving a different game an entirely
different chrome — is a definition change, never a runtime edit.
"""

from dataclasses import dataclass, field


@dataclass
class BorderStyle:
    """Box chrome, named after res tiles_data.h UI_TILE_* macros
    (lowercased, without the prefix) and palettes.h PAL_* macros."""
    top: str = "ovl_border_top"
    bottom: str = "ovl_border_bot"
    left: str = "ovl_border_left"
    right: str = "ovl_border_right"
    corner_tl: str = "ovl_corner_tl"
    corner_tr: str = "ovl_corner_tr"
    corner_bl: str = "ovl_corner_bl"
    corner_br: str = "ovl_corner_br"
    fill: str = "blank"
    palette: str = "silver"

    def c_tile(self, name):
        return f"UI_TILE_{getattr(self, name).upper()}"

    @property
    def c_palette(self):
        return f"PAL_{self.palette.upper()}"


@dataclass
class Menu:
    """Arrow-cursor menu drawn with the UI letter tiles.

    items are (label, value) pairs; labels are A-Z/0-9 only (UI tile
    font). The arrow sits at pos, labels at label_x, one item every
    row_step rows. UP/DOWN/SELECT move, START confirms.
    """
    name: str
    pos: tuple            # (arrow_x, first_row_y)
    label_x: int
    items: list
    row_step: int = 2


@dataclass
class TitleScreen:
    """The boot screen: a 16x16-per-letter logo strip (res asset),
    a decorative palette bar, and the mode menu. Palettes are OWN
    data — deliberately independent of the sprite editor's bg
    palettes so re-skins can't shift the logo colors (each entry is
    4 RGB555 (r,g,b) tuples)."""
    menu: Menu
    logo_letters: int = 7          # tiles = letters * 4
    logo_pos: tuple = (3, 4)
    deco_pos: tuple = (4, 7)
    deco_palettes: tuple = ("fire", "fire", "gold", "gold",
                            "earth", "earth", "water", "water",
                            "silver", "silver", "dark", "dark")
    # Row order MUST match the res contract's PAL_* indices (the
    # screen attributes name palettes through PAL_FIRE..PAL_SILVER).
    # The legacy table kept its pre-refactor order after PAL_SILVER
    # moved from 4 to 7 (ecc125c), which silently turned the title
    # text's silver-gray ramp into Dark/Obsidian purple for months.
    palettes: tuple = (
        ((31, 31, 31), (29, 21, 9), (28, 6, 2), (21, 9, 5)),      # 0 fire
        ((31, 31, 31), (10, 19, 22), (7, 15, 22), (2, 5, 7)),     # 1 water
        ((31, 31, 31), (14, 20, 10), (4, 18, 4), (4, 6, 4)),      # 2 earth
        ((31, 31, 31), (26, 18, 10), (18, 12, 6), (6, 4, 2)),     # 3 bronze
        ((31, 31, 31), (18, 10, 24), (10, 4, 16), (2, 0, 4)),     # 4 dark
        ((31, 31, 31), (31, 28, 8), (26, 20, 4), (6, 4, 2)),      # 5 gold
        ((31, 31, 31), (20, 20, 20), (10, 10, 10), (0, 0, 0)),    # 6 grayscale
        ((31, 31, 31), (20, 20, 20), (16, 20, 24), (4, 4, 6)),    # 7 silver
    )


@dataclass
class Overlay:
    """A boxed text overlay.

    pos/size are in bkg tile coordinates. lines are (row_within_box,
    text) pairs, drawn centered in the box interior with the given
    font (vwf_init_<font> must exist).
    """
    name: str
    pos: tuple
    size: tuple
    lines: list = field(default_factory=list)
    border: BorderStyle = field(default_factory=BorderStyle)
    font: str = "merlin"
