/* gbforge runtime — the ONE swap choreography. See flow.h. */

/* Banked placement under GBDK: these functions are __banked
   (see engine.h) so they live outside HOME, which the ng ROM
   nearly filled. Host builds ignore this. */
#ifdef __SDCC
#pragma bank 2   /* bank 1 is at capacity in both trees */
#endif

#include "engine.h"
#include "anim.h"
#include "flow.h"
#include "sound_glue.h"
#include "vram.h"

uint8_t flow_first_max_run;
uint8_t flow_any_4plus;
/* flow_swap already ran rt_find to validate the swap; the resolve
   loop's first pass reuses that result (a second scan cost ~124
   scanlines of frozen cursor per swap) */
static uint8_t flow_prefound;

uint8_t flow_swap(rt_engine *e, const ng_mode_config *cfg,
                  uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
                  uint8_t (*tile_for)(rt_engine *e,
                                      uint8_t x, uint8_t y)) BANKED
{
    uint8_t passes;
    uint8_t tmp;

    /* sprite slide (visual board lands exchanged) */
    rta_play_swap(x1, y1, x2, y2);

    tmp = e->board[y1][x1];
    e->board[y1][x1] = e->board[y2][x2];
    e->board[y2][x2] = tmp;

    if (!rt_find(e)) {
        /* no match — slide back and revert */
        rta_play_swap(x2, y2, x1, y1);
        tmp = e->board[y1][x1];
        e->board[y1][x1] = e->board[y2][x2];
        e->board[y2][x2] = tmp;
        return 0;
    }

    flow_prefound = 1;   /* matched[] / last_max_run are fresh */
    passes = flow_resolve(e, cfg, tile_for);
    return passes;
}

uint8_t flow_resolve(rt_engine *e, const ng_mode_config *cfg,
                     uint8_t (*tile_for)(rt_engine *e,
                                         uint8_t x, uint8_t y)) BANKED
{
    uint8_t passes = 0;

    flow_first_max_run = 0;
    flow_any_4plus = 0;

    /* cascade: each pass resolves instantly in the engine, then the
       interpreter plays its event script. Refill events are appended
       into the same pass buffer so playback reveals them in order. */
    do {
        e->event_count = 0;
        if (flow_prefound) flow_prefound = 0;
        else if (!rt_find(e)) break;
        /* feedback first: the flash goes up before the (slow) resolve,
           which then runs under the flash hold. One pump between the
           scan and the flash staging: together they sit right at the
           frame budget and overran on some passes. */
        if (e->yield) e->yield();
        rta_flash_early(e->matched);
        if (passes == 0) flow_first_max_run = e->last_max_run;
        if (e->last_max_run >= 4) flow_any_4plus = 1;

        /* match / chain audio: element from the first matched
           tile, size from the longest run in the pass */
        {
            uint8_t x, y, t = RT_EMPTY;
            for (y = 0; y < RT_H && t == RT_EMPTY; y++)
                for (x = 0; x < RT_W; x++)
                    if (e->matched[y][x]) { t = e->board[y][x]; break; }
            if (t >= RT_METAL_FIRST) {
                ngau_event(NGAU_EV_METAL_MATCH);
            } else if (e->last_max_run >= 5) {
                ngau_event(NGAU_EV_MATCH_5);
            } else {
                uint8_t base = (e->last_max_run >= 4)
                                   ? NGAU_EV_MATCH_4_FIRE
                                   : NGAU_EV_MATCH_3_FIRE;
                ngau_event(base + (t - 1));   /* fire/water/earth */
            }
            if (passes >= 1) {
                uint8_t c = passes;
                if (c > 4) c = 4;             /* chain_5 caps */
                ngau_event(NGAU_EV_CHAIN_2 + (c - 1));
            }
        }
        /* shake on a long first-pass run (legacy main.c:57) */
        if (passes == 0 && cfg->shake_run &&
            e->last_max_run >= cfg->shake_run) {
            rta_start_shake();
        }
        /* the match/chain jingles above are written to the APU
           synchronously (up to ~450 register writes) — pump before the
           process phase so the two never share a frame */
        if (e->yield) e->yield();
        rt_process(e);
        if (e->yield) e->yield();
        rt_gravity(e);
        if (cfg->refill && tile_for) {
            rt_refill(e, tile_for);
        }
        if (e->yield) e->yield();
        rta_play(e);
        passes++;
    } while (1);

    /* shake on a deep cascade (legacy main.c:87, passes >= 2 means a
       3-chain: swap pass + 2 cascade passes) */
    if (cfg->shake_passes && passes > cfg->shake_passes) {
        rta_start_shake();
    }

    return passes;
}
