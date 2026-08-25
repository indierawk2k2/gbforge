#ifndef GBFORGE_FLOW_H_INCLUDED
#define GBFORGE_FLOW_H_INCLUDED

/* gbforge runtime — THE swap→match→cascade choreography.
 *
 * The original game hand-wrote this sequence six times (main.c, puzzle,
 * timed x2, battle x2) and the copies drifted: puzzle lost its shake,
 * practice dropped a spell hook, cursor snap-back existed in only
 * three. Here it exists ONCE; per-mode differences are data
 * (ng_mode_config, emitted by gen_modes from the game definition).
 */

#include <stdint.h>

#include "engine.h"

#ifndef BANKED
#define BANKED
#endif

typedef struct {
    uint8_t refill;        /* refill after gravity (endless-style)   */
    uint8_t shake_run;     /* shake at match run >= n (0 = never)    */
    uint8_t shake_passes;  /* shake at cascade passes >= n (0=never) */
} ng_mode_config;

/* Attempt the swap under the given mode config. Returns the number
 * of cascade passes (0 = no match; the swap was reverted).
 * tile_for supplies refill tiles when cfg->refill (may be NULL for
 * puzzle-style modes). */
uint8_t flow_swap(rt_engine *e, const ng_mode_config *cfg,
                  uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
                  uint8_t (*tile_for)(rt_engine *e,
                                      uint8_t x, uint8_t y)) BANKED;

/* Longest run in the LAST resolve's first pass (battle bonus-turn
 * rule: a 4+ match grants another move). */
extern uint8_t flow_first_max_run;

/* 1 if ANY pass of the last resolve contained a 4+ run — the
 * legacy battle bonus-turn flag (had_4plus checks every pass,
 * not just the swap's own match). */
extern uint8_t flow_any_4plus;

/* Run the cascade loop on the current board state (used by both
 * swaps and ability casts). Returns passes. */
uint8_t flow_resolve(rt_engine *e, const ng_mode_config *cfg,
                     uint8_t (*tile_for)(rt_engine *e,
                                         uint8_t x, uint8_t y)) BANKED;

#endif
