/* gbforge runtime — full-screen cast effect interpreter.
 *
 * Everything here restores what it touches: SCX/SCY return to the
 * caller's values, BG palettes return to bg_palettes. Sprites are
 * hidden for palette-wide kinds (flash/tint/grayfade) so OBJ CRAM
 * never needs a snapshot, and shown again before returning; the
 * caller should rtc_invalidate() so the cursor redraws itself.
 */
#pragma bank 3   /* casts are cold; bank 1 is the engine's */

#include <gb/gb.h>
#include <gb/cgb.h>

#include "fx.h"
#include "ui_core.h"
#include "palettes.h"

extern const rt_fx_spec ng_ability_fx[];
extern const ng_overlay *const ng_ability_ovl[];

/* 32-entry sine, -8..8 — scaled by amplitude then >>3 */
static const int8_t fx_sin[32] = {
    0, 2, 3, 5, 6, 7, 8, 8, 8, 8, 7, 6, 5, 3, 2, 0,
    0, -2, -3, -5, -6, -7, -8, -8, -8, -8, -7, -6, -5, -3, -2, 0,
};

static void fx_frames(uint8_t n)
{
    while (n--) vsync();
}

/* ── palette helpers (CRAM writes right after vsync) ─────────── */

static void fx_fill_pal(uint16_t c)
{
    uint16_t buf[4];
    uint8_t i;
    buf[0] = buf[1] = buf[2] = buf[3] = c;
    vsync();
    for (i = 0; i < 8; i++) set_bkg_palette(i, 1, buf);
}

static void fx_restore_pal(void)
{
    vsync();
    set_bkg_palette(0, 8, bg_palettes);
}

/* lerp all 32 colors toward (r,g,b); k = 0..8 */
static void fx_tint_step(uint8_t r, uint8_t g, uint8_t b, uint8_t k)
{
    uint16_t buf[32];
    uint8_t i;
    for (i = 0; i < 32; i++) {
        uint16_t c = bg_palettes[i];
        uint8_t cr = (uint8_t)(c & 31);
        uint8_t cg = (uint8_t)((c >> 5) & 31);
        uint8_t cb = (uint8_t)((c >> 10) & 31);
        cr = (uint8_t)(cr + (((int8_t)(r - cr) * k) >> 3));
        cg = (uint8_t)(cg + (((int8_t)(g - cg) * k) >> 3));
        cb = (uint8_t)(cb + (((int8_t)(b - cb) * k) >> 3));
        buf[i] = (uint16_t)(cr | ((uint16_t)cg << 5)
                            | ((uint16_t)cb << 10));
    }
    vsync();
    set_bkg_palette(0, 8, buf);
}

/* desaturate all 32 colors toward their own luma; k = 0..8 */
static void fx_gray_step(uint8_t k)
{
    uint16_t buf[32];
    uint8_t i;
    for (i = 0; i < 32; i++) {
        uint16_t c = bg_palettes[i];
        uint8_t cr = (uint8_t)(c & 31);
        uint8_t cg = (uint8_t)((c >> 5) & 31);
        uint8_t cb = (uint8_t)((c >> 10) & 31);
        uint8_t y = (uint8_t)((cr + cg + cb) / 3);
        cr = (uint8_t)(cr + (((int8_t)(y - cr) * k) >> 3));
        cg = (uint8_t)(cg + (((int8_t)(y - cg) * k) >> 3));
        cb = (uint8_t)(cb + (((int8_t)(y - cb) * k) >> 3));
        buf[i] = (uint16_t)(cr | ((uint16_t)cg << 5)
                            | ((uint16_t)cb << 10));
    }
    vsync();
    set_bkg_palette(0, 8, buf);
}

/* ── raster helper: one frame of per-line SCX from the sine ───── */

static void fx_raster_frame(uint8_t base_scx, int8_t amp,
                            uint8_t phase, uint8_t shear_split)
{
    uint8_t prev = 0xFF;
    uint8_t ly;
    vsync();
    /* vsync() returns INSIDE vblank (LY 144-153) — wait for the
       wrap to line 0 or the visible-line loop below never runs
       (aqua/mercury shipped with no visible effect at all) */
    while (LY_REG >= 144)
        ;
    /* visible lines only; a missed line just softens the effect */
    while ((ly = LY_REG) < 144) {
        if (ly != prev) {
            prev = ly;
            if (shear_split) {
                int8_t off = (int8_t)((fx_sin[phase & 31] * amp) >> 3);
                SCX_REG = (uint8_t)(base_scx +
                    ((ly < (uint8_t)(shear_split << 3)) ? off
                                                        : (int8_t)-off));
            } else {
                SCX_REG = (uint8_t)(base_scx +
                    ((fx_sin[(uint8_t)(ly + phase) & 31] * amp) >> 3));
            }
        }
    }
    SCX_REG = base_scx;
}

/* ── the interpreter ──────────────────────────────────────────── */

void rtfx_run(const rt_fx_spec *fx) BANKED
{
    uint8_t sx = SCX_REG, sy = SCY_REG;
    uint8_t f = fx->frames;
    uint8_t t;

    switch (fx->kind) {

    case RTFX_FLASH: {
        uint8_t pulses = fx->a ? fx->a : 2;
        uint16_t c = fx->b ? RGB(0, 0, 0) : RGB(31, 31, 31);
        uint8_t hold = (uint8_t)(f / (pulses << 1));
        if (!hold) hold = 2;
        HIDE_SPRITES;
        for (t = 0; t < pulses; t++) {
            fx_fill_pal(c);
            fx_frames(hold);
            fx_restore_pal();
            fx_frames(hold);
        }
        SHOW_SPRITES;
        break;
    }

    case RTFX_SHAKE: {
        /* decaying jitter off a coarse LFSR — bigger and longer
           than the 5-chain shake */
        uint8_t seed = 0xA7;
        for (t = 0; t < f; t++) {
            uint8_t amp = (uint8_t)(fx->a - ((fx->a * t) / f));
            seed = (uint8_t)((seed << 1) ^ (seed & 0x80 ? 0x1D : 0));
            vsync();
            SCX_REG = (uint8_t)(sx + (seed & (amp | 1)) - (amp >> 1));
            SCY_REG = (uint8_t)(sy + ((seed >> 3) & (amp | 1))
                                - (amp >> 1));
        }
        vsync();
        SCX_REG = sx;
        SCY_REG = sy;
        break;
    }

    case RTFX_WAVE: {
        for (t = 0; t < f; t++) {
            int8_t amp = (int8_t)(fx->a - ((fx->a * t) / f));
            fx_raster_frame(sx, amp, (uint8_t)(t * fx->b), 0);
        }
        break;
    }

    case RTFX_SHEAR: {
        for (t = 0; t < f; t++) {
            int8_t amp = (int8_t)(fx->a - ((fx->a * t) / f));
            fx_raster_frame(sx, amp, (uint8_t)(t << 1),
                            fx->b ? fx->b : 9);
        }
        break;
    }

    case RTFX_TINT: {
        uint8_t half = (uint8_t)(f >> 1);
        HIDE_SPRITES;
        for (t = 0; t < half; t++) {
            fx_tint_step(fx->a, fx->b, fx->c,
                         (uint8_t)((t << 3) / half));
        }
        for (t = half; t > 0; t--) {
            fx_tint_step(fx->a, fx->b, fx->c,
                         (uint8_t)(((t - 1) << 3) / half));
        }
        fx_restore_pal();
        SHOW_SPRITES;
        break;
    }

    case RTFX_TRIFADE: {
        /* one tint pulse per manna color — fire red, water blue,
           earth green — then the promotion lands on the reveal */
        static const uint8_t tri[3][3] = {
            { 31, 8, 8 }, { 8, 14, 31 }, { 10, 26, 10 },
        };
        uint8_t third = (uint8_t)(f / 3);
        uint8_t half = (uint8_t)(third >> 1);
        uint8_t c2;
        if (!half) half = 4;
        HIDE_SPRITES;
        for (c2 = 0; c2 < 3; c2++) {
            for (t = 0; t < half; t++) {
                fx_tint_step(tri[c2][0], tri[c2][1], tri[c2][2],
                             (uint8_t)((t << 3) / half));
            }
            for (t = half; t > 0; t--) {
                fx_tint_step(tri[c2][0], tri[c2][1], tri[c2][2],
                             (uint8_t)(((t - 1) << 3) / half));
            }
        }
        fx_restore_pal();
        SHOW_SPRITES;
        break;
    }

    case RTFX_GRAYFADE: {
        uint8_t half = (uint8_t)(f >> 1);
        HIDE_SPRITES;
        for (t = 0; t < half; t++) {
            fx_gray_step((uint8_t)((t << 3) / half));
        }
        fx_frames(6);   /* hold the drained look a beat */
        for (t = half; t > 0; t--) {
            fx_gray_step((uint8_t)(((t - 1) << 3) / half));
        }
        fx_restore_pal();
        SHOW_SPRITES;
        break;
    }

    default:
        break;
    }
}

void rtfx_cast(uint8_t idx) BANKED
{
    const ng_overlay *o = ng_ability_ovl[idx];
    const rt_fx_spec *fx = &ng_ability_fx[idx];
    if (fx->kind == RTFX_NONE && !o) return;
    if (o) {
        ui_show_overlay(o);
        fx_frames(8);       /* let the name land before the effect */
    }
    rtfx_run(fx);
    if (o) fx_frames(6);
    /* no hide pass: the caller's board blit repaints the cells the
       overlay covered — the reveal IS the spell taking effect */
}
