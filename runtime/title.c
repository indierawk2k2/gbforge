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

static void draw_arrow(const ng_menu *m, uint8_t row, uint8_t tile)
{
    uint8_t t = tile;
    set_bkg_tiles(m->arrow_x, m->first_y + row * m->row_step, 1, 1, &t);
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

    /* logo strip: N 16x16 letters, 4 tiles each */
    {
        uint8_t top_row[16], bot_row[16];
        uint8_t w = t->logo_letters << 1;
        for (i = 0; i < t->logo_letters; i++) {
            uint8_t base = TITLE_LOGO_TILE_BASE + (i << 2);
            top_row[(i << 1)] = base;
            top_row[(i << 1) + 1] = base + 1;
            bot_row[(i << 1)] = base + 2;
            bot_row[(i << 1) + 1] = base + 3;
        }
        set_bkg_tiles(t->logo_x, t->logo_y, w, 1, top_row);
        set_bkg_tiles(t->logo_x, t->logo_y + 1, w, 1, bot_row);
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

    /* menu labels + arrow */
    for (i = 0; i < t->menu.count; i++) {
        set_bkg_tiles(t->menu.label_x,
                      t->menu.first_y + i * t->menu.row_step,
                      t->menu.items[i].len, 1,
                      (uint8_t *)t->menu.items[i].tiles);
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
