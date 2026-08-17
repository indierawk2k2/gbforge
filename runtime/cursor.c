/* gbforge runtime — cursor bracket. See cursor.h. Exact port of
 * legacy render.c render_cursor + input.c cursor_animate. */

#ifdef __SDCC
#pragma bank 2
#endif

#include <gb/gb.h>
#include <gb/cgb.h>

#include "cursor.h"
#include "sprites_data.h"
#include "palettes.h"

#define CURSOR_SPEED 6
#define CURSOR_PAL_TARGETING 7
#define SCREEN_OFF 3   /* board pixels start at (3,3) under SCX/SCY=5 */

int16_t cursor_px = 0, cursor_py = 0;

static int16_t last_px = -1, last_py = -1;
static uint8_t last_mode = 0xFF;
static uint8_t last_tile_base = 0xFF;

void rtc_snap(uint8_t gx, uint8_t gy) BANKED
{
    cursor_px = (int16_t)gx << 4;
    cursor_py = (int16_t)gy << 4;
}

uint8_t rtc_animate(uint8_t gx, uint8_t gy) BANKED
{
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
    uint8_t tile_x = (uint8_t)cursor_px + SCREEN_OFF;
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
