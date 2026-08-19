/* Per-band BG palette streaming (NONBANKED — the LYC ISR runs with
 * arbitrary banks mapped, so everything here lives in HOME).
 *
 * Scheme: an image spans N 8-pixel bands. Bands alternate palette
 * slot halves — even bands render from BG palettes 0-2, odd bands
 * from 3-5 — so while band b-1 is on screen its half is idle and
 * band b's three palettes are streamed into it, one palette per
 * HBlank window (mode-3 locks CRAM; we edge-sync on mode-3 exit so
 * every 8-byte write lands inside mode0+mode2, ~167+ dots at the
 * slowest). Slot 6 is untouched; slot 7 stays the UI palette.
 *
 * VBlank loads bands 0 and 1; an LYC interrupt walking down the
 * frame loads each next band, then restores the caller's palettes
 * below the image (the row directly below a streamed image must be
 * blank/UI-palette — the last restore lands during that row).
 *
 * Data lives in ROM banks; bandpal_start() copies the palette block
 * to WRAM so the ISR never touches bank registers.
 */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "bandpal.h"

uint8_t bp_active;
uint8_t bp_line;                     /* first image scanline */
uint8_t bp_endline;                  /* line after the image */
uint8_t bp_bands;
uint8_t bp_phase;                    /* next band / restore phase */
uint8_t bp_palbuf[8 * 24];   /* bands x 3 pals x 8B */
uint8_t bp_restore[56];              /* palettes 0-6 */
uint8_t bp_has_restore;
uint8_t bp_installed;

/* write one palette, mode-guarded (raster only; vblank writes free).
 * Everything slow happens BEFORE the wait: the 8 bytes are preloaded
 * into locals and BCPS (a register, writable in any mode) is set up
 * front, so only 8 tight stores sit inside the mode0+mode2 window
 * (>=167 dots) — spilling into mode 3 would silently drop bytes. */
static void bp_wpal(uint8_t slot, const uint8_t *p)
{
    uint8_t b0 = p[0], b1 = p[1], b2 = p[2], b3 = p[3];
    uint8_t b4 = p[4], b5 = p[5], b6 = p[6], b7 = p[7];
    BCPS_REG = 0x80 | (slot << 3);
    if (LY_REG < 143) {
        while ((STAT_REG & 3) != 3);        /* wait for mode 3 */
        while ((STAT_REG & 3) == 3);        /* its end: window opens */
    }
    BCPD_REG = b0; BCPD_REG = b1; BCPD_REG = b2; BCPD_REG = b3;
    BCPD_REG = b4; BCPD_REG = b5; BCPD_REG = b6; BCPD_REG = b7;
}

static void bp_rest3(uint8_t half)
{
    bp_wpal(half, &bp_restore[half * 8]);
    bp_wpal(half + 1, &bp_restore[(half + 1) * 8]);
    bp_wpal(half + 2, &bp_restore[(half + 2) * 8]);
}

static void bp_write_band(uint8_t band)
{
    const uint8_t *src = &bp_palbuf[band * 24];
    uint8_t base = (band & 1) ? 3 : 0;
    bp_wpal(base, src);
    bp_wpal(base + 1, src + 8);
    bp_wpal(base + 2, src + 16);
}

/* load bands 0/1 and schedule what follows */
static void bp_lead(void)
{
    bp_write_band(0);
    if (bp_bands > 1) bp_write_band(1);
    bp_phase = 2;
    if (bp_bands > 2) {
        LYC_REG = bp_line + 8;                  /* band 1's lines */
    } else if (bp_has_restore) {
        LYC_REG = bp_endline - 8;
        bp_phase = bp_bands;
    }
}

void bp_vbl(void)
{
    if (!bp_active) return;
    if (bp_has_restore) {
        /* content ABOVE the image renders normal palettes: restore
           the full set in vblank; bands 0/1 stream during the
           spacer row above the image (phase 0) */
        bp_rest3(0);
        bp_rest3(3);
        bp_wpal(6, &bp_restore[48]);
        bp_phase = 0;
        LYC_REG = bp_line - 8;
        return;
    }
    bp_lead();
}

void bp_lcd(void)
{
    uint8_t ph = bp_phase;
    if (!bp_active) return;
    if (ph == 0) {
        /* lead-in during the spacer row above: bands 0 and 1 */
        bp_lead();
        return;
    }
    if (ph < bp_bands) {
        /* load band ph while band ph-1 renders */
        bp_write_band(ph);
        bp_phase = ph + 1;
        if (ph + 1 < bp_bands) {
            LYC_REG = bp_line + (ph << 3);
        } else if (bp_has_restore) {
            LYC_REG = bp_endline - 8;
        }
    } else if (bp_has_restore && ph == bp_bands) {
        /* during the last band: restore the idle half + slot 6 */
        bp_rest3((bp_bands & 1) ? 3 : 0);   /* not used by last */
        bp_wpal(6, &bp_restore[48]);
        bp_phase = ph + 1;
        LYC_REG = bp_endline;               /* spacer row */
    } else if (bp_has_restore && ph == bp_bands + 1) {
        /* below the image (blank/UI spacer row): the used half */
        bp_rest3((bp_bands & 1) ? 0 : 3);
        bp_phase = ph + 1;                  /* idle until next vblank */
    }
}


/* HOME half ends here — bandpal_start/stop and the blitters live in
 * bandpal2.c (banked): they never run in interrupt context, and the
 * debug build's HOME budget is tight. */
