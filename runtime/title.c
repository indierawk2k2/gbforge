/* gbforge runtime — title screen + arrow menu. See title.h. */

#ifdef __SDCC
#pragma bank 2   /* bank 1 is the engine's; the title screen is cold */
#endif

#include <gb/gb.h>
#include <gb/cgb.h>

#include "title.h"
#include "sound_glue.h"
#include "vram.h"
#include "debug.h"

#include "tiles_data.h"
#include "palettes.h"
#include "title_logo.h"   /* TITLE_LOGO_TILE_BASE — res contract */
#include "merlin_font.h"   /* VWF_POOL_BASE — res contract */

static void draw_arrow(const ng_menu *m, uint8_t row, uint8_t tile)
{
    uint8_t t = tile;
    set_bkg_tiles(m->items[row].arrow_x,
                  m->first_y + row * m->row_step, 1, 1, &t);
}

uint8_t rtt_show(const ng_title *t) BANKED
{
    uint8_t y, i;
    uint8_t sel = 0;
    uint8_t prev_keys, keys, pressed;

    DISPLAY_OFF;
    SCX_REG = 0;
    SCY_REG = 0;
    HIDE_WIN;
    HIDE_SPRITES;
    set_bkg_palette(0, 8, t->palettes);
    rtv_load_title_gfx();

    /* clear screen */
    VBK_REG = VBK_ATTRIBUTES;
    {
        uint8_t attr_row[20];
        for (i = 0; i < 20; i++) attr_row[i] = PAL_SILVER;
        for (y = 0; y < 18; y++) set_bkg_tiles(0, y, 20, 1, attr_row);
    }
    VBK_REG = VBK_TILES;
    {
        uint8_t blank_row[20];
        for (i = 0; i < 20; i++) blank_row[i] = UI_TILE_BLANK;
        for (y = 0; y < 18; y++) set_bkg_tiles(0, y, 20, 1, blank_row);
    }

    /* logo: one upload of the baked two-row run, then point the map
       at it. The generator centred the INK, so logo_x is wherever
       that landed rather than a number anyone chose. */
    {
        uint8_t row[20];
        uint8_t n = t->logo_tiles;
        uint8_t base = TITLE_LOGO_TILE_BASE;
        set_bkg_data(base, n << 1, t->logo);
        for (i = 0; i < n && i < 20; i++) row[i] = base + i;
        set_bkg_tiles(t->logo_x, t->logo_y, n, 1, row);
        for (i = 0; i < n && i < 20; i++) row[i] = base + n + i;
        set_bkg_tiles(t->logo_x, t->logo_y + 1, n, 1, row);
    }

    /* decorative palette bar (deco_n == 0 declares no bar) */
    if (t->deco_n) {
        uint8_t deco[16];
        for (i = 0; i < t->deco_n; i++) deco[i] = UI_TILE_SOLID;
        VBK_REG = VBK_ATTRIBUTES;
        set_bkg_tiles(t->deco_x, t->deco_y, t->deco_n, 1,
                      (uint8_t *)t->deco_attrs);
        VBK_REG = VBK_TILES;
        set_bkg_tiles(t->deco_x, t->deco_y, t->deco_n, 1, deco);
    }

    /* menu labels: baked pixel-centered runs streamed through the
       VWF pool (36 tiles for three items — pool holds 46) */
    {
        uint8_t base = VWF_POOL_BASE;
        uint8_t buf[20];
        uint8_t c;
        for (i = 0; i < t->menu.count; i++) {
            const ng_menu_item *it = &t->menu.items[i];
            set_bkg_data(base, it->n_tiles, it->tiles);
            for (c = 0; c < it->n_tiles && c < 20; c++) buf[c] = base + c;
            set_bkg_tiles(it->run_x,
                          t->menu.first_y + i * t->menu.row_step,
                          it->n_tiles, 1, buf);
            base += it->n_tiles;
        }
    }
    draw_arrow(&t->menu, 0, UI_TILE_ARROW);

    SHOW_BKG;
    DISPLAY_ON;

    prev_keys = joypad();
    while (1) {
        vsync();
        keys = joypad();
        pressed = keys & ~prev_keys;
        prev_keys = keys;

#ifdef DEBUG_BUILD
        if (debug_req != DBG_NONE) {
            DISPLAY_OFF;
            return 0xFF;  /* caller services the mailbox */
        }
#endif

        if (pressed & J_A) { ngau_event(NGAU_EV_MENU_CONFIRM); break; }
        if (pressed & (J_UP | J_DOWN | J_SELECT)) {
            ngau_event(NGAU_EV_MENU_MOVE);
            draw_arrow(&t->menu, sel, UI_TILE_BLANK);
            if (pressed & (J_DOWN | J_SELECT)) {
                sel = (sel == t->menu.count - 1) ? 0 : sel + 1;
            } else {
                sel = (sel == 0) ? t->menu.count - 1 : sel - 1;
            }
            draw_arrow(&t->menu, sel, UI_TILE_ARROW);
        }
    }

    DISPLAY_OFF;
    return t->menu.items[sel].value;
}
