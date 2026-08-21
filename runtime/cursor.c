/* gbforge runtime — cursor bracket. See cursor.h. Exact port of
 * legacy render.c render_cursor + input.c cursor_animate. */

#ifdef __SDCC
#pragma bank 2
#endif

#include <gb/gb.h>
#include <gb/cgb.h>

#include "cursor.h"
#include "vram.h"     /* rt_screen_off_x */
#include "sprites_data.h"
#include "palettes.h"

#define CURSOR_SPEED 6
#define CURSOR_PAL_TARGETING 7
#define SCREEN_OFF 3   /* board pixels start at (3,3) under SCX/SCY=5 */

int16_t cursor_px = 0, cursor_py = 0;
static uint8_t ride_lock;   /* this frame's position is authored by
                               an animation (swap slide) — skip the
                               glide step so bracket and tile move
                               on the SAME easing curve */

static int16_t last_px = -1, last_py = -1;
static uint8_t last_mode = 0xFF;
static uint8_t last_tile_base = 0xFF;

void rtc_ride(int16_t px, int16_t py) BANKED
{
    cursor_px = px;
    cursor_py = py;
    ride_lock = 1;
}

void rtc_snap(uint8_t gx, uint8_t gy) BANKED
{
    cursor_px = (int16_t)gx << 4;
    cursor_py = (int16_t)gy << 4;
}

uint8_t rtc_animate(uint8_t gx, uint8_t gy) BANKED
{
    if (ride_lock) {
        ride_lock = 0;
        return 1;
    }
    int16_t tx = (int16_t)gx << 4;
    int16_t ty = (int16_t)gy << 4;
    int16_t dx = tx - cursor_px;
    int16_t dy = ty - cursor_py;

    if (dx > CURSOR_SPEED) cursor_px += CURSOR_SPEED;
    else if (dx < -CURSOR_SPEED) cursor_px -= CURSOR_SPEED;
    else cursor_px = tx;

    if (dy > CURSOR_SPEED) cursor_py += CURSOR_SPEED;
    else if (dy < -CURSOR_SPEED) cursor_py -= CURSOR_SPEED;
    else cursor_py = ty;

    return (cursor_px != tx || cursor_py != ty) ? 1 : 0;
}

void rtc_invalidate(void) BANKED
{
    last_px = -1;
    last_mode = 0xFF;
    last_tile_base = 0xFF;
}

void rtc_draw(uint8_t frame, uint8_t mode) BANKED
{
    uint8_t tile_x = (uint8_t)cursor_px + rt_screen_off_x;
    uint8_t tile_y = (uint8_t)cursor_py + SCREEN_OFF;
    uint8_t tile_base;
    uint8_t pal = 0;

    if (mode == 1) {
        tile_base = CURSOR_TILE_NORMAL;
        pal = CURSOR_PAL_TARGETING;
    } else if (mode == 2) {
        tile_base = (frame & 0x04) ? CURSOR_TILE_INVERTED
                                   : CURSOR_TILE_NORMAL;
    } else if (mode == 3 || mode == 4) {
        tile_base = (frame & 0x04) ? CURSOR_TILE_INVERTED
                                   : CURSOR_TILE_NORMAL;
        pal = CURSOR_PAL_TARGETING;
    } else if (mode == 5 || mode == 6) {
        tile_base = CURSOR_TILE_NORMAL;
        pal = PAL_CURSOR_OPPONENT;
    } else {
        tile_base = CURSOR_TILE_NORMAL;
        pal = CURSOR_PAL_TARGETING;
    }

    if (mode != last_mode) {
        last_mode = mode;
        if (mode == 0) {
            uint16_t p[4] = { RGB(0,0,0), RGB(31,31,31), RGB(24,24,26),
                              RGB(0,0,0) };
            set_sprite_palette(CURSOR_PAL_TARGETING, 1, p);
        } else if (mode == 1) {
            uint16_t p[4] = { RGB(0,0,0), RGB(20,20,20), RGB(14,14,16),
                              RGB(0,0,0) };
            set_sprite_palette(CURSOR_PAL_TARGETING, 1, p);
        } else if (mode == 3) {
            uint16_t p[4] = { RGB(0,0,0), RGB(10,16,31), RGB(6,10,24),
                              RGB(0,0,0) };
            set_sprite_palette(CURSOR_PAL_TARGETING, 1, p);
        } else if (mode == 4) {
            uint16_t p[4] = { RGB(0,0,0), RGB(31,6,6), RGB(24,4,4),
                              RGB(0,0,0) };
            set_sprite_palette(CURSOR_PAL_TARGETING, 1, p);
        } else if (mode == 5) {
            uint16_t p[4] = { RGB(0,0,0), RGB(31,6,6), RGB(22,4,4),
                              RGB(0,0,0) };
            set_sprite_palette(PAL_CURSOR_OPPONENT, 1, p);
        } else if (mode == 6) {
            uint16_t p[4] = { RGB(0,0,0), RGB(20,3,3), RGB(12,2,2),
                              RGB(0,0,0) };
            set_sprite_palette(PAL_CURSOR_OPPONENT, 1, p);
        }
        set_sprite_prop(CURSOR_SPRITE_BASE, pal);
        set_sprite_prop(CURSOR_SPRITE_BASE + 1, pal);
        set_sprite_prop(CURSOR_SPRITE_BASE + 2, pal);
        set_sprite_prop(CURSOR_SPRITE_BASE + 3, pal);
        last_tile_base = 0xFF;
    }

    if (cursor_px == last_px && cursor_py == last_py &&
        tile_base == last_tile_base) {
        return;
    }
    last_px = cursor_px;
    last_py = cursor_py;
    last_tile_base = tile_base;

    {
        int8_t inset = (mode == 1 || mode == 6) ? 1 : 0;

        set_sprite_tile(CURSOR_SPRITE_BASE, tile_base);
        move_sprite(CURSOR_SPRITE_BASE,
                    tile_x - 3 + 8 + inset, tile_y - 2 + 16 + inset);

        set_sprite_tile(CURSOR_SPRITE_BASE + 1, tile_base + 1);
        move_sprite(CURSOR_SPRITE_BASE + 1,
                    tile_x + 11 + 8 - inset, tile_y - 2 + 16 + inset);

        set_sprite_tile(CURSOR_SPRITE_BASE + 2, tile_base + 2);
        move_sprite(CURSOR_SPRITE_BASE + 2,
                    tile_x - 3 + 8 + inset, tile_y + 10 + 16 - inset);

        set_sprite_tile(CURSOR_SPRITE_BASE + 3, tile_base + 3);
        move_sprite(CURSOR_SPRITE_BASE + 3,
                    tile_x + 11 + 8 - inset, tile_y + 10 + 16 - inset);
    }
}

/* ── ghost hint cursors (legacy render_hint_cursors port) ────── */

void rtc_hint_palettes(void) BANKED
{
    uint16_t pal_opp[4] = { RGB(0, 0, 0), RGB(31, 6, 6),
                            RGB(22, 4, 4), RGB(0, 0, 0) };
    uint16_t pal_hint[4] = { RGB(0, 0, 0), RGB(18, 18, 18),
                             RGB(10, 10, 10), RGB(0, 0, 0) };
    uint16_t pal_unsolv[4] = { RGB(0, 0, 0), RGB(31, 6, 6),
                               RGB(22, 4, 4), RGB(0, 0, 0) };
    uint16_t pal_score[4] = { RGB(0, 0, 0), RGB(20, 20, 20),
                              RGB(28, 5, 5), RGB(0, 0, 0) };
    /* slots 4-6 are exactly the palettes the hint wiggle borrows
       for its ghost tile; reloading all three here makes this the
       one restore point (PAL_CURSOR_OPPONENT doubles as the battle
       CPU cursor and opponent-counter red) */
    set_sprite_palette(PAL_CURSOR_OPPONENT, 1, pal_opp);
    set_sprite_palette(PAL_GHOST_HINT, 1, pal_hint);
    set_sprite_palette(PAL_GHOST_UNSOLVABLE, 1, pal_unsolv);
    /* OBJ 7: opponent score — red glyphs, standard gray shadow */
    set_sprite_palette(7, 1, pal_score);
}

static void ghost_bracket(uint8_t base, uint8_t gx, uint8_t gy,
                          uint8_t pal)
{
    uint8_t tx = (uint8_t)((gx << 4) + rt_screen_off_x);
    uint8_t ty = (uint8_t)((gy << 4) + SCREEN_OFF);
    uint8_t i;

    for (i = 0; i < 4; i++) {
        set_sprite_tile(base + i, CURSOR_TILE_NORMAL + i);
        set_sprite_prop(base + i, pal);
    }
    move_sprite(base,     tx - 3 + 8,  ty - 2 + 16);
    move_sprite(base + 1, tx + 11 + 8, ty - 2 + 16);
    move_sprite(base + 2, tx - 3 + 8,  ty + 10 + 16);
    move_sprite(base + 3, tx + 11 + 8, ty + 10 + 16);
}

void rtc_hint_cursors(uint8_t x1, uint8_t y1,
                      uint8_t x2, uint8_t y2,
                      uint8_t unsolvable) BANKED
{
    uint8_t pal = unsolvable ? PAL_GHOST_UNSOLVABLE : PAL_GHOST_HINT;
    ghost_bracket(GHOST_CURSOR_A_BASE, x1, y1, pal);
    ghost_bracket(GHOST_CURSOR_B_BASE, x2, y2, pal);
}

void rtc_hint_hide(void) BANKED
{
    uint8_t i;
    for (i = 0; i < 4; i++) {
        move_sprite(GHOST_CURSOR_A_BASE + i, 0, 0);
        move_sprite(GHOST_CURSOR_B_BASE + i, 0, 0);
    }
}
