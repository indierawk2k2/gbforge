/* gbforge runtime — punitive reset. See reset.h. Exact port of
 * legacy board.c board_punitive_reset onto rt_engine. */

#ifdef __SDCC
#pragma bank 2
#endif

#include "reset.h"

void rt_punitive_reset(rt_engine *e,
                       uint8_t (*tile_for)(rt_engine *e,
                                           uint8_t x, uint8_t y),
                       uint8_t (*coord3)(void)) BANKED
{
    uint8_t x, y;
    uint8_t highest = RT_EMPTY;
    uint8_t safety;
    uint8_t saved_metals[RT_H * RT_W];
    uint8_t metal_count = 0;

    for (y = 0; y < RT_H; y++)
        for (x = 0; x < RT_W; x++)
            if (e->board[y][x] > highest)
                highest = e->board[y][x];

    if (highest == RT_EMPTY) return;

    /* Remove the highest tier entirely; bank the other metals. */
    for (y = 0; y < RT_H; y++) {
        for (x = 0; x < RT_W; x++) {
            if (e->board[y][x] == highest) {
                e->board[y][x] = RT_EMPTY;
            } else if (e->board[y][x] > RT_METAL_FIRST) {
                saved_metals[metal_count++] = e->board[y][x];
                e->board[y][x] = RT_EMPTY;
            }
        }
    }

    for (safety = 0; safety < 5; safety++) {
        for (y = 0; y < RT_H; y++)
            for (x = 0; x < RT_W; x++)
                if (e->board[y][x] == RT_EMPTY)
                    e->board[y][x] = tile_for(e, x, y);

        /* survivors at random spots (legacy rand()&7 order: x, y) */
        {
            uint8_t i;
            for (i = 0; i < metal_count; i++) {
                uint8_t px, py, attempts = 0;
                do {
                    px = coord3() & 7;   /* mask defensively */
                    py = coord3() & 7;
                    attempts++;
                } while (e->board[py][px] > RT_METAL_FIRST &&
                         attempts < 50);
                e->board[py][px] = saved_metals[i];
            }
        }

        if (rt_has_legal_moves(e)) return;

        for (y = 0; y < RT_H; y++)
            for (x = 0; x < RT_W; x++)
                e->board[y][x] = RT_EMPTY;
    }
}
