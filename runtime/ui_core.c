/* gbforge runtime — boxed-overlay primitives. See ui_core.h. */

/* Banked placement under GBDK: these functions are __banked
   (see engine.h) so they live outside HOME, which the ng ROM
   nearly filled. Host builds ignore this. */
#ifdef __SDCC
#pragma bank 4   /* overlay drawing is cold; bank 1 is the engine's */
#endif

#include <gb/gb.h>
#include <gb/cgb.h>

#include "ui_core.h"

#include "merlin_font.h"     /* VWF_POOL_BASE — res contract    */

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
    uint8_t i, c;
    uint8_t base = VWF_POOL_BASE;
    uint8_t buf[20];

    ui_box(&o->box);
    /* Lines are baked at build time: pixel-perfect centering, no
       runtime VWF. The tile data lives in this bank (generated
       ui_overlays.c carries the same #pragma bank). */
    for (i = 0; i < o->line_count; i++) {
        const ng_overlay_line *ln = &o->lines[i];
        set_bkg_data(base, ln->n_tiles, ln->tiles);
        for (c = 0; c < ln->n_tiles && c < 20; c++) buf[c] = base + c;
        set_bkg_tiles((uint8_t)(o->box.x + 1 + ln->dx),
                      (uint8_t)(o->box.y + ln->dy),
                      ln->n_tiles, 1, buf);
        base += ln->n_tiles;
    }
}
