/* Host-native transcript oracle for the runtime engine.
 *
 * runtime/engine.c is written to be PURE: board in, event list out,
 * no VRAM and no hardware. That is what lets the same translation
 * unit compile here with -DBANKED= and run ten thousand boards in
 * under a second, instead of ten thousand emulated cascades.
 *
 * The transcript is an FNV-1a hash over every intermediate state the
 * resolver passes through — match mask, run length, dirty rows,
 * transmute count, post-process board, resources, post-gravity board.
 * A hash rather than a golden file because the interesting property
 * is "did ANY step change", and because the same walk is implemented
 * independently in gbforge/engine/sim.py: two implementations that
 * agree on a 64-bit fold of ~1.6M observations are agreeing on the
 * semantics, not on a test fixture.
 *
 * Build and compare with `make -C tests`.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"

static uint64_t fnv = 1469598103934665603ULL;

static void put(uint8_t b)
{
    fnv = (fnv ^ b) * 1099511628211ULL;
}

static void put_board(const rt_engine *e)
{
    uint8_t x, y;
    for (y = 0; y < RT_H; y++)
        for (x = 0; x < RT_W; x++)
            put(e->board[y][x]);
}

/* Same xorshift32 the Python side runs, seeded the same way — the
   boards must be identical or the hashes are meaningless. */
static uint32_t xs_state;

static uint32_t xs(void)
{
    uint32_t x = xs_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    xs_state = x;
    return x;
}

static uint8_t count_transmutes(const rt_engine *e)
{
    uint8_t i, n = 0;
    for (i = 0; i < e->event_count; i++)
        if (e->events[i].type == RT_EV_TRANSMUTE) n++;
    return n;
}

int main(int argc, char **argv)
{
    long boards = (argc > 1) ? strtol(argv[1], NULL, 10) : 10000;
    long total_passes = 0, total_groups = 0;
    long n;

    for (n = 0; n < boards; n++) {
        rt_engine e;
        uint8_t x, y;

        xs_state = (uint32_t)(0x9E3779B9u ^ (uint32_t)n);

        rt_init(&e);
        for (y = 0; y < RT_H; y++) {
            for (x = 0; x < RT_W; x++) {
                uint32_t r = xs();
                e.board[y][x] = (r % 8 < 5) ? (uint8_t)(1 + r % 3)
                                            : (uint8_t)(4 + ((r >> 8) % 5));
            }
        }

        put_board(&e);

        /* The event list is per-PASS: rt_resolve_pass clears it before
         * each find/process/gravity. Driving the three calls directly
         * (so each step can be observed) means clearing it here too —
         * otherwise a pass with no transmutes still counts the previous
         * pass's, and the two implementations disagree about a
         * bookkeeping detail rather than about the game. */
        e.event_count = 0;
        while (rt_find(&e)) {
            total_passes++;
            for (y = 0; y < RT_H; y++)
                for (x = 0; x < RT_W; x++)
                    put(e.matched[y][x]);
            put(e.last_max_run);
            put(rt_dirty_rows(&e));

            total_groups += rt_process(&e);
            put(count_transmutes(&e));
            put_board(&e);
            put(e.manna[0]);
            put(e.manna[1]);
            put(e.manna[2]);
            put((uint8_t)(e.knowledge & 0xFF));
            put((uint8_t)(e.knowledge >> 8));

            rt_gravity(&e);
            put_board(&e);
            e.event_count = 0;
        }
    }

    printf("boards=%ld passes=%ld groups=%ld\n",
           boards, total_passes, total_groups);
    printf("transcript_hash=%016llx\n", (unsigned long long)fnv);
    return 0;
}
