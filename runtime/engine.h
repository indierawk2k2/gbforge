#ifndef GBFORGE_ENGINE_H_INCLUDED
#define GBFORGE_ENGINE_H_INCLUDED

/* gbforge runtime engine — match/transmute/gravity/refill as PURE
 * STATE TRANSITIONS that emit a RESOLUTION SCRIPT (event list).
 *
 * This is the load-bearing architectural change from legacy match.c:
 * the legacy render_gravity_drop() mutated board[][] WHILE animating,
 * entangling timing with logic. Here the engine resolves a full step
 * into rt_events[], and the animation interpreter (anim.c) plays the
 * script afterwards at whatever timing the AnimationSpec dictates.
 * Logic is testable off-screen; animations are tunable without
 * touching semantics.
 *
 * Semantics are transcript-locked to gbforge/engine/sim.py and legacy
 * match.c (see tests/runtime_transcript.c — hash must equal the
 * golden corpus fingerprint).
 *
 * GBC constraints: no malloc, fixed-width types, static buffers.
 * Compiles host-native for tests (-DBANKED=) and under GBDK.
 */

#include <stdint.h>

/* Under GBDK/SDCC, BANKED must be the real __banked keyword — an
 * empty fallback here would silently break the calling convention
 * for any TU that includes runtime headers before a GBDK header. */
#ifdef __SDCC
#include <asm/types.h>
#else
#ifndef BANKED
#define BANKED
#endif
#endif

#define RT_W 8
#define RT_H 8

/* Tile types mirror tiles_data.h (the editor-owned contract). */
#define RT_EMPTY   0
#define RT_MANNA_FIRST 1   /* FIRE..EARTH  */
#define RT_MANNA_LAST  3
#define RT_METAL_FIRST 4   /* BRONZE..AETHER */
#define RT_METAL_LAST  11
#define RT_TILE_TYPES  12

/* ── Resolution script ─────────────────────────────── */

typedef enum {
    RT_EV_PASS_BEGIN,   /* a=max_run, b=dirty_rows            */
    RT_EV_MATCH_ROW,    /* a=y, b=bitmask of matched columns   */
    RT_EV_AWARD_MANNA,  /* a=tile type, b=amount (pre-cap),
                           c=(center_x<<4)|center_y — the flood
                           centroid, where the +N float spawns  */
    RT_EV_AWARD_KNOW,   /* a=lo, b=hi of knowledge points      */
    RT_EV_TRANSMUTE,    /* a=x, b=y, c=new tile                */
    RT_EV_BONUS,        /* a=x, b=y, c=new tile (4-chain bonus)*/
    RT_EV_FALL,         /* a=x, b=from_y, c=to_y               */
    RT_EV_REFILL,       /* a=x, b=y, c=tile                    */
    RT_EV_PASS_END      /* a=groups this pass                  */
} rt_event_type;

typedef struct {
    uint8_t type;
    uint8_t a, b, c;
} rt_event;

/* Worst case per pass: header + 8 row masks + awards + transmutes +
 * up to 64 falls + up to 64 refills. */
#define RT_MAX_EVENTS 208

/* ── Engine state ──────────────────────────────────── */

typedef struct {
    uint8_t manna_cap;            /* 0 = default 99            */
    uint8_t manna_bonus[3];       /* fire, water, earth        */
    uint8_t knowledge_pct_bonus;
    uint8_t chain_bonus_pct;      /* 0 disables the 4-chain roll */
} rt_effects;

typedef struct {
    uint8_t board[RT_H][RT_W];
    uint8_t matched[RT_H][RT_W];
    uint8_t max_tier;             /* transmutation cap (RT_METAL_LAST) */
    rt_effects fx;

    uint8_t manna[3];             /* fire, water, earth */
    uint16_t knowledge;
    uint16_t cleared_total;       /* tiles cleared, lifetime — a
                                     generic score source for games
                                     without resource pools */

    uint8_t last_max_run;

    /* rng(n) in [0,n) — only consumed by the 4-chain bonus when
       chain_bonus_pct > 0. NULL = bonus disabled. */
    uint8_t (*rng)(uint8_t n);

    rt_event events[RT_MAX_EVENTS];
    uint8_t event_count;
} rt_engine;

/* ── API ───────────────────────────────────────────── */

void rt_init(rt_engine *e) BANKED;

/* match_find: mark runs, set last_max_run. Returns 1 if any. */
uint8_t rt_find(rt_engine *e) BANKED;

/* match_process: clear + award + transmute; emits PASS_BEGIN,
   MATCH_ROW, AWARD_*, TRANSMUTE/BONUS events. Returns group count. */
uint8_t rt_process(rt_engine *e) BANKED;

/* board gravity; emits FALL events. Returns 1 if anything moved. */
uint8_t rt_gravity(rt_engine *e) BANKED;

/* refill empty cells top-down using tile_for(x, y) (game-supplied
   generation policy); emits REFILL events. */
void rt_refill(rt_engine *e, uint8_t (*tile_for)(rt_engine *e,
                                                 uint8_t x,
                                                 uint8_t y)) BANKED;

/* One full pass: find→process→gravity (no refill — puzzle semantics).
   Clears the event buffer first. Returns groups, 0 = stable. */
uint8_t rt_resolve_pass(rt_engine *e) BANKED;

uint8_t rt_dirty_rows(const rt_engine *e) BANKED;
uint8_t rt_has_legal_moves(rt_engine *e) BANKED;

/* First match-producing move in scan order (legacy find_hint_move):
 * sets (x, y) of the tile to nudge. Returns 1 if found. */
uint8_t rt_find_hint(rt_engine *e, uint8_t *x, uint8_t *y) BANKED;
uint8_t rt_next_tier(const rt_engine *e, uint8_t tile) BANKED;
uint16_t rt_knowledge_for_tier(uint8_t tile) BANKED;

/* Count of match-producing adjacent swaps (legacy
 * count_available_moves: right+down per cell, both-ends check). */
uint8_t rt_count_moves(const rt_engine *e) BANKED;

#endif /* GBFORGE_ENGINE_H_INCLUDED */
