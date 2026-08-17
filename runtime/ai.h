#ifndef GBFORGE_AI_H_INCLUDED
#define GBFORGE_AI_H_INCLUDED

/* gbforge runtime — the CPU opponent's move search (legacy
 * battle_ai.c port onto rt_engine): greedy over all adjacent swaps,
 * scored by match size (4+ weighted x3) with a metal-tier bonus as
 * the tiebreak. */

#include <stdint.h>

#include "engine.h"

#ifdef __SDCC
#include <asm/types.h>
#else
#ifndef BANKED
#define BANKED
#endif
#endif

/* Best adjacent swap on e's board. Returns 1 if any match-producing
 * move exists (0 = deadlocked). */
uint8_t rt_ai_best_swap(rt_engine *e,
                        uint8_t *x1, uint8_t *y1,
                        uint8_t *x2, uint8_t *y2) BANKED;

#endif
