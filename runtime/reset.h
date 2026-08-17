#ifndef GBFORGE_RESET_H_INCLUDED
#define GBFORGE_RESET_H_INCLUDED

/* gbforge runtime — dead-board punitive reset (legacy
 * board_punitive_reset port): destroy every tile of the highest
 * tier on the board, preserve all other metals above bronze,
 * refill the gaps, and re-place the survivors at random positions.
 * Retries up to 5 fresh layouts until the board has a legal move.
 * Player resources are untouched — that's the caller's contract.
 */

#include <stdint.h>

#include "engine.h"

#ifdef __SDCC
#include <asm/types.h>
#else
#ifndef BANKED
#define BANKED
#endif
#endif

/* tile_for: same callback contract as rt_refill (would-match retry
 * inside). coord3: must return ((uint8_t)rand()) & 7 — the legacy
 * CAST-FIRST semantics. e->rng (rand() % n) is NOT equivalent:
 * GBDK rand() is signed, so % of a negative byte yields huge
 * uint8 values — wrong placement and out-of-bounds writes. */
void rt_punitive_reset(rt_engine *e,
                       uint8_t (*tile_for)(rt_engine *e,
                                           uint8_t x, uint8_t y),
                       uint8_t (*coord3)(void)) BANKED;

#endif
