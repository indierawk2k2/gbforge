/* Banked half of the per-band palette streamer: setup + blitters.
 * Never runs in interrupt context — the ISRs live in bandpal.c
 * (HOME). Bank 3 has headroom and these paths are cold. */
#pragma bank 3

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "bandpal.h"

/* HOME cross-bank copy helper (lib/vwf) — OLDCALL like the def */
void vwf_memcpy(void *dst, const void *src, uint16_t len,
                uint8_t bank) OLDCALL;

extern uint8_t bp_active, bp_line, bp_endline, bp_bands, bp_phase;
extern uint8_t bp_palbuf[8 * 24];
extern uint8_t bp_restore[56];
extern uint8_t bp_has_restore, bp_installed;
void bp_vbl(void);
void bp_lcd(void);

void bandpal_start(uint8_t bank, const uint16_t *pals, uint8_t bands,
                   uint8_t y0, const uint16_t *restore) BANKED
{
    uint8_t i;
    const uint8_t *s;

    bp_active = 0;
    if (restore) {
        s = (const uint8_t *)restore;
        for (i = 0; i < 56; i++) bp_restore[i] = s[i];
        bp_has_restore = 1;
    } else {
        bp_has_restore = 0;
    }
    /* cross-bank read via the HOME helper — this function's own
       code is banked, so it must NEVER switch ROM itself */
    vwf_memcpy(bp_palbuf, pals, (uint16_t)bands * 24, bank);

    bp_line = (uint8_t)(y0 << 3);
    bp_endline = (uint8_t)((y0 + bands) << 3);
    bp_bands = bands;
    bp_phase = 2;

    if (!bp_installed) {
        bp_installed = 1;
        CRITICAL {
            add_VBL(bp_vbl);
            add_LCD(bp_lcd);
            STAT_REG |= 0x40;               /* LYC source */
        }
    }
    LYC_REG = (uint8_t)((y0 + 1) << 3);   /* first LCD irq target */
    CRITICAL { IE_REG |= LCD_IFLAG; }
    bp_active = 1;
}

void bandpal_stop(void) BANKED
{
    bp_active = 0;
    CRITICAL { IE_REG &= (uint8_t)~LCD_IFLAG; }
    /* leave CRAM as-is: callers repaint or restore palettes next */
}

/* blit one 8-tile-row slice of a streamed image; sized so a call
 * per vsync stays inside VBlank (display-on staging). vmap gives
 * the VRAM slot per tile index, or NULL for vbase + index. */
void bandpal_blit_row(uint8_t bank, const uint8_t *tiles,
                      const uint8_t *attrs, uint8_t x, uint8_t y,
                      uint8_t w, uint8_t row, const uint8_t *vmap,
                      uint8_t vbase, uint8_t vbk1) BANKED
{
    uint8_t map[20];
    uint8_t att[20];
    uint8_t buf[16];
    uint8_t col, idx, vslot;

    for (col = 0; col < w; col++) {
        idx = (uint8_t)(row * w + col);
        vslot = vmap ? vmap[idx] : (uint8_t)(vbase + idx);
        vwf_memcpy(buf, &tiles[(uint16_t)idx << 4], 16, bank);
        if (vbk1) VBK_REG = 1;
        set_bkg_data(vslot, 1, buf);
        VBK_REG = 0;
        map[col] = vslot;
        vwf_memcpy(&att[col], &attrs[idx], 1, bank);
        att[col] |= (vbk1 ? 0x08 : 0);
    }
    VBK_REG = VBK_ATTRIBUTES;
    set_bkg_tiles(x, (uint8_t)(y + row), w, 1, att);
    VBK_REG = VBK_TILES;
    set_bkg_tiles(x, (uint8_t)(y + row), w, 1, map);
}

/* full blit — display off or don't care about tearing */
void bandpal_blit(uint8_t bank, const uint8_t *tiles,
                  const uint8_t *attrs, uint8_t x, uint8_t y,
                  uint8_t w, uint8_t bands, const uint8_t *vmap,
                  uint8_t vbase, uint8_t vbk1) BANKED
{
    uint8_t row;
    for (row = 0; row < bands; row++) {
        bandpal_blit_row(bank, tiles, attrs, x, y, w, row,
                         vmap, vbase, vbk1);
    }
}
