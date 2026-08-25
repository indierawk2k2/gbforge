"""Emit the title screen + menu data (gen/title_config.{h,c}).

Menu labels are BAKED against the res VWF font: pixel-centered tile
runs (same pipeline as overlays), with each item's arrow position
computed from where its ink actually starts. Deco palettes compile
to PAL_* symbols; the title palette set to an RGB555 table — colors
are definition data, deliberately independent of the sprite
editor's bg palettes (the original game's rainbow-bar
guarantee, now structural).
"""

import os

import re

from gbforge.codegen.gen_ui import MerlinFont, _fmt_bytes

MENU_RUN_TILES = 12   # baked run per item; 3 items = 36 pool tiles
ARROW_GAP_PX = 12     # gap between the arrow and the label ink
SCREEN_TILES = 20


def _arrow_ink(out_dir):
    """(x0, x1) ink bounds of the menu arrow inside its 8px cell,
    exported by the res pack. The art owns its own padding; the
    layout just needs to know about it."""
    path = os.path.join(out_dir, "..", "res", "tiles_data.h")
    src = open(path).read()
    m0 = re.search(r"#define\s+UI_TILE_ARROW_INK_X0\s+(\d+)", src)
    m1 = re.search(r"#define\s+UI_TILE_ARROW_INK_X1\s+(\d+)", src)
    if not m0 or not m1:
        return (0, 7)      # unknown padding: treat the cell as solid
    return (int(m0.group(1)), int(m1.group(1)))


class TitleFont:
    """The 16x16 display weight from res/merlin_font.c.

    Same 1bpp encoding as the 8px face, two bytes per row. Kept
    separate from MerlinFont because a title logo is composed into a
    TWO-tile-row run and the 8px baker only knows about one.
    """

    def __init__(self, recode, widths, bitmaps):
        self.recode = recode
        self.widths = widths
        self.bitmaps = bitmaps     # list of 16 (hi, lo) row pairs

    @classmethod
    def load(cls, out_dir):
        path = os.path.join(out_dir, "..", "res", "merlin_font.c")
        if not os.path.exists(path):
            return None
        src = open(path).read()

        def table(name):
            m = re.search(rf"{name}\[\d*\]\s*=\s*{{(.*?)}};", src, re.S)
            if not m:
                return None
            body = re.sub(r"//[^\n]*|/\*.*?\*/", "", m.group(1), flags=re.S)
            return [int(t, 0) for t in
                    re.findall(r"0[xX][0-9a-fA-F]+|\b\d+\b", body)]

        recode, widths, bits = (table("title_recode"), table("title_widths"),
                                table("title_bitmaps"))
        if not recode or not widths or not bits:
            return None
        glyphs = [[(bits[g * 32 + r * 2], bits[g * 32 + r * 2 + 1])
                   for r in range(16)]
                  for g in range(len(bits) // 32)]
        return cls(recode, widths, glyphs)

    def width_px(self, text):
        """Total advance, trailing sidebearing included."""
        return sum(self.widths[self.recode[ord(c)]]
                   for c in text if ord(c) < 128)

    def _glyph_ink(self, gi):
        cols = [b for b in range(16)
                if any((hi if b < 8 else lo) & (0x80 >> (b % 8))
                       for hi, lo in self.bitmaps[gi])]
        return (min(cols), max(cols)) if cols else (0, 0)

    def ink_width_px(self, text):
        """Width of the actual marks, ignoring the sidebearing after
        the last glyph. Centring on the advance instead leaves the
        run visibly off by that bearing."""
        chars = [c for c in text if ord(c) < 128]
        if not chars:
            return 0
        pen = 0
        first_ink = None
        last_ink = 0
        for c in chars:
            gi = self.recode[ord(c)]
            x0, x1 = self._glyph_ink(gi)
            if any(any(r) for r in self.bitmaps[gi]):
                if first_ink is None:
                    first_ink = pen + x0
                last_ink = pen + x1
            pen += self.widths[gi]
        if first_ink is None:
            return 0
        return last_ink - first_ink + 1

    def ink_lead_px(self, text):
        """Left bearing of the first inked glyph."""
        for c in text:
            if ord(c) > 127:
                continue
            gi = self.recode[ord(c)]
            if any(any(r) for r in self.bitmaps[gi]):
                return self._glyph_ink(gi)[0]
        return 0

    def bake(self, text, x0):
        """Compose `text` with its ink starting at pixel x0 within a
        run of whole tiles. Returns (tiles_wide, flat 2bpp bytes) for
        a TWO-row run, tile-column major within each row — the order
        set_bkg_data wants for one upload.

        Ink is colour 3 with a +1/+1 colour-1 drop shadow, the same
        treatment the overlay baker gives 8px text, so the two weights
        look like one family.
        """
        total = x0 + self.width_px(text)
        tiles = (total + 7) // 8
        W, H = tiles * 8, 16
        ink = [[0] * W for _ in range(H)]

        pen = x0
        for ch in text:
            if ord(ch) > 127:
                continue
            g = self.bitmaps[self.recode[ord(ch)]]
            for y in range(16):
                hi, lo = g[y]
                for b in range(16):
                    on = (hi & (0x80 >> b)) if b < 8 else (lo & (0x80 >> (b - 8)))
                    if on and 0 <= pen + b < W:
                        ink[y][pen + b] = 1
            pen += self.widths[self.recode[ord(ch)]]

        shadow = [[0] * W for _ in range(H)]
        for y in range(H - 1):
            for x in range(W - 1):
                if ink[y][x]:
                    shadow[y + 1][x + 1] = 1

        data = []
        for row in range(2):
            for t in range(tiles):
                for r in range(8):
                    y = row * 8 + r
                    lo_b = hi_b = 0
                    for b in range(8):
                        x = t * 8 + b
                        if ink[y][x]:
                            lo_b |= 0x80 >> b
                            hi_b |= 0x80 >> b
                        elif shadow[y][x]:
                            lo_b |= 0x80 >> b
                    data += [lo_b, hi_b]
        return tiles, data

HEADER = """\
/* Auto-generated by gbforge (gen_title) — do not edit. */
#ifndef GEN_TITLE_CONFIG_H_INCLUDED
#define GEN_TITLE_CONFIG_H_INCLUDED

#include "title.h"

extern const ng_title ng_title_screen;

#endif
"""

SOURCE = """\
/* Auto-generated by gbforge (gen_title) — do not edit.
 * Same bank as title.c: it uploads the baked label tiles while
 * this bank is mapped. */
#ifdef __SDCC
#pragma bank 2
#endif

#include "title.h"
#include "title_config.h"

#include "tiles_data.h"
#include "palettes.h"

#define RGB555(r, g, b) ((uint16_t)((r) | ((g) << 5) | ((b) << 10)))

static const uint16_t title_palettes[{npal} * 4] = {{
{palettes}
}};

/* "{logo_text}" baked in the 16x16 display weight: {logo_px}px of ink,
 * centred on the 160px screen at pixel precision. Row-major — the
 * whole run uploads with one set_bkg_data. */
static const uint8_t logo_run[{logo_bytes}] = {{
{logo_data}
}};

{labels}
static const ng_menu_item menu_items[{nitems}] = {{
{items}
}};

const ng_title ng_title_screen = {{
    logo_run, {logo_tiles}, {logo_x}, {logo_y},
    {deco_x}, {deco_y}, {deco_n},
    {{ {deco_attrs} }},
    title_palettes,
    {{ {menu_y}, {row_step}, {nitems}, menu_items }}
}};
"""


def _label_tiles(label):
    out = []
    for ch in label.upper():
        if "A" <= ch <= "Z":
            out.append(f"UI_TILE_LETTER_A + {ord(ch) - ord('A')}")
        elif "0" <= ch <= "9":
            out.append(f"UI_TILE_DIGIT_0 + {ord(ch) - ord('0')}")
        else:
            out.append("UI_TILE_BLANK")
    return out


def emit(game, out_dir):
    t = getattr(game, "title", None)
    if t is None:
        return []
    os.makedirs(out_dir, exist_ok=True)

    pal_rows = []
    for pal in t.palettes:
        row = ", ".join(f"RGB555({r}, {g}, {b})" for r, g, b in pal)
        pal_rows.append(f"    {row},")
    palettes = "\n".join(pal_rows)

    font = MerlinFont.load(out_dir)
    if font is None:
        raise SystemExit(
            "gen_title: res/merlin_font.c not found — baked menu "
            "labels require the VWF font source")

    # ── the logo ────────────────────────────────────────────────
    title_font = TitleFont.load(out_dir)
    if title_font is None:
        raise SystemExit(
            "gen_title: res/merlin_font.c has no 16x16 display weight "
            "(title_recode/title_widths/title_bitmaps)")

    logo_text = (t.logo_text or game.name).upper()
    logo_px = title_font.ink_width_px(logo_text)
    if title_font.width_px(logo_text) > SCREEN_TILES * 8:
        raise SystemExit(
            f"gen_title: logo '{logo_text}' is {logo_px}px, screen is "
            f"{SCREEN_TILES * 8}px — shorten it or narrow the face")
    # Centre the INK, then work back to which tile the run starts in
    # and how far into that tile the first pixel sits. Centring the
    # advance width instead leaves the run off by the last glyph's
    # sidebearing; centring the RUN quantises it to 8px, which is what
    # put the old monospaced strip visibly right of centre.
    logo_pen = ((SCREEN_TILES * 8 - logo_px) // 2
                - title_font.ink_lead_px(logo_text))
    logo_x = logo_pen // 8
    logo_tiles, logo_data = title_font.bake(logo_text, logo_pen - logo_x * 8)
    if logo_x + logo_tiles > SCREEN_TILES:
        raise SystemExit(
            f"gen_title: logo run overflows the screen "
            f"({logo_x}+{logo_tiles} > {SCREEN_TILES})")

    # ── menu items ──────────────────────────────────────────────
    # The menu is laid out as one BLOCK, not as independently centred
    # rows: arrows share a column, labels share a left edge, and the
    # block as a whole is optically centred on the widest label. Rows
    # centred individually would stagger the arrows, which reads as
    # misalignment however carefully each row was centred.
    arrow_ink = _arrow_ink(out_dir)
    widths = [font.width_px(label) for label, _ in t.menu.items]
    for (label, _), px in zip(t.menu.items, widths):
        if px > MENU_RUN_TILES * 8:
            raise SystemExit(
                f"gen_title: menu label '{label}' is {px}px, run is "
                f"{MENU_RUN_TILES * 8}px")
    # Measured in INK, not advance: the block has to line up with the
    # arrow, and the sidebearing after a label's last glyph is not
    # something a reader can see.
    widest_ink = max(font.ink_width_px(label) for label, _ in t.menu.items)
    lead = min(font.ink_lead_px(label) for label, _ in t.menu.items)

    # Block ink spans from the arrow's first mark to the widest
    # label's last. The arrow is a background tile and can only land
    # on an 8px boundary; the labels are baked and can land anywhere.
    #
    # So the arrow snaps, and the GAP absorbs the snapping error —
    # the labels are then placed to make the block's ink exactly
    # symmetric. Letting the label follow the snapped arrow instead
    # would push the whole block off centre by up to 4px, which is
    # what the eye actually reads.
    block_ink = (8 - arrow_ink[0]) + ARROW_GAP_PX + widest_ink
    block_start = (SCREEN_TILES * 8 - block_ink) // 2
    arrow_x = (block_start - arrow_ink[0] + 4) // 8      # tile-aligned
    if arrow_x < 0:
        arrow_x = 0

    visible_left = arrow_x * 8 + arrow_ink[0]
    ink_end = (SCREEN_TILES * 8 - 1) - visible_left      # mirror it
    ink_start = ink_end - widest_ink + 1 - lead

    gap = ink_start + lead - (arrow_x * 8 + 8)
    if gap < 4:
        raise SystemExit(
            f"gen_title: menu labels leave only {gap}px beside the "
            f"arrow — the block is too wide for the screen")

    run_x = ink_start // 8
    if run_x + MENU_RUN_TILES > SCREEN_TILES:
        run_x = SCREEN_TILES - MENU_RUN_TILES

    labels = ""
    item_rows = []
    for i, ((label, value), px) in enumerate(zip(t.menu.items, widths)):
        data = font.bake_line_at(label, MENU_RUN_TILES,
                                 ink_start - run_x * 8)
        labels += (f'/* "{label}" — {px}px advance, left edge at screen '
                   f"pixel {ink_start} (gap {gap}px after the arrow) */\n"
                   f"static const uint8_t label_{i}[{len(data)}] = {{\n"
                   f"{_fmt_bytes(data)}\n}};\n")
        item_rows.append(
            f"    {{ label_{i}, {MENU_RUN_TILES}, {run_x}, "
            f"{value}, {arrow_x} }}")

    # An empty bar still needs a valid C initializer; the runtime
    # skips drawing entirely when deco_n is 0.
    deco_attrs = (", ".join(f"PAL_{p.upper()}" for p in t.deco_palettes)
                  or "0")

    with open(os.path.join(out_dir, "title_config.h"), "w") as f:
        f.write(HEADER)
    with open(os.path.join(out_dir, "title_config.c"), "w") as f:
        f.write(SOURCE.format(
            npal=len(t.palettes),
            palettes=palettes,
            labels=labels,
            nitems=len(t.menu.items),
            items=",\n".join(item_rows),
            logo_text=logo_text,
            logo_px=logo_px,
            logo_bytes=len(logo_data),
            logo_data=_fmt_bytes(logo_data),
            logo_tiles=logo_tiles,
            logo_x=logo_x, logo_y=t.logo_pos[1],
            deco_x=t.deco_pos[0], deco_y=t.deco_pos[1],
            deco_n=len(t.deco_palettes),
            deco_attrs=deco_attrs,
            menu_y=t.menu.pos[1],
            row_step=t.menu.row_step,
        ))
    return ["title_config.h", "title_config.c"]
