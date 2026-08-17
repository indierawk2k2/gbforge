/* gbforge runtime — boxed-overlay primitives. See ui_core.h. */

/* Banked placement under GBDK: these functions are __banked
   (see engine.h) so they live outside HOME, which the ng ROM
   nearly filled. Host builds ignore this. */
#ifdef __SDCC
#pragma bank 1
#endif

#include <gb/gb.h>
#include <gb/cgb.h>

#include "ui_core.h"
#include "tiles_data.h"


void ui_box(const ng_box_style *s) BANKED
{
    uint8_t buf[20];
    uint8_t c, r;

    /* palette attributes for the whole box */
    VBK_REG = VBK_ATTRIBUTES;
    for (c = 0; c < s->w && c < 20; c++) buf[c] = s->pal;
    for (r = 0; r < s->h; r++)
        set_bkg_tiles(s->x, s->y + r, s->w, 1, buf);

    VBK_REG = VBK_TILES;

    buf[0] = s->c_tl;
    for (c = 1; c < s->w - 1; c++) buf[c] = s->t_top;
    buf[s->w - 1] = s->c_tr;
    set_bkg_tiles(s->x, s->y, s->w, 1, buf);

    buf[0] = s->t_left;
    for (c = 1; c < s->w - 1; c++) buf[c] = s->fill;
    buf[s->w - 1] = s->t_right;
    for (r = 1; r < s->h - 1; r++)
        set_bkg_tiles(s->x, s->y + r, s->w, 1, buf);

    buf[0] = s->c_bl;
    for (c = 1; c < s->w - 1; c++) buf[c] = s->t_bot;
    buf[s->w - 1] = s->c_br;
    set_bkg_tiles(s->x, s->y + s->h - 1, s->w, 1, buf);
}

void ui_show_overlay(const ng_overlay *o) BANKED
{
    uint8_t i;

    ui_box(&o->box);
    for (i = 0; i < o->line_count; i++) {
        const ng_overlay_line *ln = &o->lines[i];
        const char *s = ln->text;
        uint8_t len = 0;
        uint8_t buf[18];
        uint8_t x;

        while (s[len] && len < (uint8_t)(o->box.w - 2)) {
            char c = s[len];
            if (c >= 'A' && c <= 'Z')
                buf[len] = UI_TILE_LETTER_A + (c - 'A');
            else if (c >= '0' && c <= '9')
                buf[len] = UI_TILE_DIGIT_0 + (c - '0');
            else
                buf[len] = UI_TILE_BLANK;
            len++;
        }
        x = o->box.x + 1 + (uint8_t)((o->box.w - 2 - len) >> 1);
        VBK_REG = VBK_TILES;
        set_bkg_tiles(x, o->box.y + ln->dy, len, 1, buf);
    }
}
