/* gbforge runtime engine — see engine.h. Transcript-locked port of
 * match.c/board.c/spells.c semantics, restructured to emit a
 * resolution script instead of driving rendering.
 *
 * Ordering is preserved bug-for-bug from match.c: horizontal scans
 * before vertical, flood-filled manna awards before metal runs,
 * row-major flood seeding, identical transmute spot selection.
 */

/* Banked placement under GBDK: these functions are __banked
   (see engine.h) so they live outside HOME, which the ng ROM
   nearly filled. Host builds ignore this. */
#ifdef __SDCC
#pragma bank 1
#endif

#include <string.h>

#include "engine.h"

/* All reward numbers come from the generated scoring tables
   (gen_scoring; declared in the game definition) — one source for
   every mode. scoring_config.c shares this bank. */
#include "scoring_config.h" 

static uint8_t check_match_at(rt_engine *e, uint8_t x, uint8_t y);

static void emit(rt_engine *e, uint8_t type, uint8_t a, uint8_t b, uint8_t c)
{
    if (e->event_count < RT_MAX_EVENTS) {
        rt_event *ev = &e->events[e->event_count++];
        ev->type = type;
        ev->a = a;
        ev->b = b;
        ev->c = c;
    }
}

void rt_init(rt_engine *e) BANKED
{
    memset(e, 0, sizeof(*e));
    e->max_tier = RT_METAL_LAST;
}

uint16_t rt_knowledge_for_tier(uint8_t tile) BANKED
{
    if (tile >= RT_TILE_TYPES) return 0;
    return ng_knowledge_per_tier[tile];
}

uint8_t rt_next_tier(const rt_engine *e, uint8_t tile) BANKED
{
    uint8_t next;
    if (tile < RT_METAL_FIRST || tile >= RT_METAL_LAST) return RT_EMPTY;
    next = tile + 1;
    if (next > e->max_tier) return RT_EMPTY;
    return next;
}

static uint8_t manna_for_run(uint8_t run)
{
    return ng_manna_for_run[run > 8 ? 8 : run];
}

static void add_manna(rt_engine *e, uint8_t tile, uint8_t amount)
{
    uint8_t cap = e->fx.manna_cap ? e->fx.manna_cap : ng_manna_cap;
    uint8_t idx = tile - RT_MANNA_FIRST;
    if (tile < RT_MANNA_FIRST || tile > RT_MANNA_LAST) return;
    if ((uint16_t)e->manna[idx] + amount > cap) e->manna[idx] = cap;
    else e->manna[idx] += amount;
}

static void add_knowledge(rt_engine *e, uint16_t amount)
{
    if (e->knowledge + amount > ng_knowledge_cap)
        e->knowledge = ng_knowledge_cap;
    else e->knowledge += amount;
}

static void award(rt_engine *e, uint8_t tile, uint8_t run, uint8_t pos)
{
    if (tile >= RT_MANNA_FIRST && tile <= RT_MANNA_LAST) {
        uint8_t amount = manna_for_run(run);
        amount += e->fx.manna_bonus[tile - RT_MANNA_FIRST];
        add_manna(e, tile, amount);
        emit(e, RT_EV_AWARD_MANNA, tile, amount, pos);
    } else if (tile >= RT_METAL_FIRST && tile <= RT_METAL_LAST) {
        uint16_t kp = ng_knowledge_per_tier[tile] * run;
        if (e->fx.knowledge_pct_bonus > 0) {
            kp += (kp * e->fx.knowledge_pct_bonus) / 100;
        }
        add_knowledge(e, kp);
        /* c carries the award centroid (x<<4|y) so the animator can
           float feedback at the match — knowledge awards used to be
           visually silent, which read as "no points" */
        emit(e, RT_EV_AWARD_KNOW, (uint8_t)(kp & 0xFF),
             (uint8_t)(kp >> 8), pos);
    }
}

/* ── find ──────────────────────────────────────────── */

#define RT_YIELD(e) do { if ((e)->yield) (e)->yield(); } while (0)

uint8_t rt_find(rt_engine *e) BANKED
{
    /* Row / column pointers throughout: SDCC turned every
       e->board[y][x] into ~100 cycles of 16-bit index math, which made
       this scan ~124 scanlines and the whole per-pass resolve ~4
       frames of frozen cursor (bead GameBoyGames-7py). Same visit
       order and results as before. */
    uint8_t x, y, type, run, i;
    uint8_t found = 0;

    memset(e->matched, 0, sizeof(e->matched));
    e->last_max_run = 0;

    /* Single 8-bit indices only: an expression like `x + run < RT_W`
       or `row[x + run]` is promoted to a signed int by C and SDCC
       spends ~50 instructions per compare on it. */
    for (y = 0; y < RT_H; y++) {
        const uint8_t *row = e->board[y];
        uint8_t *mrow = e->matched[y];
        x = 0;
        while (x < RT_W) {
            type = row[x];
            if (type == RT_EMPTY) { x++; continue; }
            i = x; i++;
            while (i < RT_W && row[i] == type) i++;
            run = i; run -= x;
            if (run >= 3) {
                uint8_t j = x;
                for (; j < i; j++) mrow[j] = 1;
                found = 1;
                if (run > e->last_max_run) e->last_max_run = run;
            }
            x = i;
        }
    }

    RT_YIELD(e);   /* horizontal scan ~50 scanlines; vertical follows */
    for (x = 0; x < RT_W; x++) {
        const uint8_t *col = &e->board[0][x];      /* stride RT_W */
        uint8_t *mcol = &e->matched[0][x];
        uint8_t idx = 0;                            /* y * RT_W */
        y = 0;
        while (y < RT_H) {
            uint8_t j, jdx;
            type = col[idx];
            if (type == RT_EMPTY) { y++; idx += RT_W; continue; }
            j = y; j++; jdx = idx; jdx += RT_W;
            while (j < RT_H && col[jdx] == type) { j++; jdx += RT_W; }
            run = j; run -= y;
            if (run >= 3) {
                uint8_t k = idx;
                for (; k < jdx; k += RT_W) mcol[k] = 1;
                found = 1;
                if (run > e->last_max_run) e->last_max_run = run;
            }
            y = j; idx = jdx;
        }
    }

    if (found) {
        uint8_t counts[RT_TILE_TYPES];
        const uint8_t *b = &e->board[0][0];
        const uint8_t *m = &e->matched[0][0];
        memset(counts, 0, sizeof(counts));
        for (i = 0; i < RT_H * RT_W; i++)
            if (m[i]) counts[b[i]]++;
        for (type = 1; type < RT_TILE_TYPES; type++)
            if (counts[type] > e->last_max_run)
                e->last_max_run = counts[type];
    }
    return found;
}

uint8_t rt_dirty_rows(const rt_engine *e) BANKED
{
    uint8_t x, y, dirty = 0, ybit = 1;
    const uint8_t *m = &e->matched[0][0];
    for (y = 0; y < RT_H; y++, ybit <<= 1) {
        for (x = 0; x < RT_W; x++, m++) {
            if (*m) { dirty |= ybit; m += (uint8_t)(RT_W - x); break; }
        }
    }
    return dirty;
}

/* ── process ───────────────────────────────────────── */

/* Scratch arrays live in static storage: as stack locals they pushed
   these frames past 127 bytes, after which SDCC can no longer address
   locals with `ldhl sp,#n` and every access costs a 16-bit add — the
   per-pass resolve was ~4 frames (bead GameBoyGames-7py). The engine
   is not reentrant, so statics are equivalent. */
static uint8_t fm_awarded[RT_H][RT_W];
static uint8_t fm_stack_x[RT_H * RT_W], fm_stack_y[RT_H * RT_W];
static uint8_t rp_saved[RT_H][RT_W];
static uint8_t cb_cand_x[RT_H * RT_W], cb_cand_y[RT_H * RT_W];

static void flood_manna(rt_engine *e, const uint8_t saved[RT_H][RT_W])
{
#define awarded fm_awarded
#define stack_x fm_stack_x
#define stack_y fm_stack_y
    uint8_t sx, sy, x, y, type, sp, count, sum_x, sum_y;

    memset(awarded, 0, sizeof(awarded));

    for (sy = 0; sy < RT_H; sy++) {
        const uint8_t *mrow = e->matched[sy];
        const uint8_t *arow = awarded[sy];
        const uint8_t *srow = saved[sy];
        for (sx = 0; sx < RT_W; sx++) {
            if (!mrow[sx] || arow[sx]) continue;
            type = srow[sx];
            if (type < RT_MANNA_FIRST || type > RT_MANNA_LAST) continue;

            sp = 0;
            stack_x[sp] = sx;
            stack_y[sp] = sy;
            sp++;
            count = 0;
            sum_x = 0;
            sum_y = 0;
            while (sp > 0) {
                sp--;
                x = stack_x[sp];
                y = stack_y[sp];
                if (awarded[y][x] || !e->matched[y][x] ||
                    saved[y][x] != type) continue;
                awarded[y][x] = 1;
                count++;
                sum_x += x;
                sum_y += y;
                if (x > 0)        { stack_x[sp] = x - 1; stack_y[sp] = y; sp++; }
                if (x < RT_W - 1) { stack_x[sp] = x + 1; stack_y[sp] = y; sp++; }
                if (y > 0)        { stack_x[sp] = x; stack_y[sp] = y - 1; sp++; }
                if (y < RT_H - 1) { stack_x[sp] = x; stack_y[sp] = y + 1; sp++; }
            }
            if (count >= 3) {
                /* flood centroid (legacy sum/count), packed for the
                   +N float's spawn cell */
                uint8_t pos = (uint8_t)(((sum_x / count) << 4)
                                        | (sum_y / count));
                award(e, type, count, pos);
            }
        }
    }
#undef awarded
#undef stack_x
#undef stack_y
}

static void try_4chain_bonus(rt_engine *e, uint8_t metal_type)
{
    uint8_t next = rt_next_tier(e, metal_type);
#define cand_x cb_cand_x
#define cand_y cb_cand_y
    uint8_t n = 0, x, y, pick;

    if (next == RT_EMPTY) return;
    if (e->rng == 0 || e->fx.chain_bonus_pct == 0) return;
    if (e->rng(100) >= e->fx.chain_bonus_pct) return;

    for (y = 0; y < RT_H; y++)
        for (x = 0; x < RT_W; x++)
            if (e->board[y][x] == metal_type) {
                cand_x[n] = x;
                cand_y[n] = y;
                n++;
            }
    if (n == 0) return;
    pick = e->rng(n);
    e->board[cand_y[pick]][cand_x[pick]] = next;
    emit(e, RT_EV_BONUS, cand_x[pick], cand_y[pick], next);
#undef cand_x
#undef cand_y
}

static void metal_run(rt_engine *e, uint8_t type, uint8_t run,
                      uint8_t x0, uint8_t y0, uint8_t horizontal)
{
    uint8_t next = rt_next_tier(e, type);
    award(e, type, run, 0);  /* knowledge awards spawn no float */
    if (next != RT_EMPTY) {
        if (run >= 5) {
            uint8_t p1 = 1, p2 = run - 2, k;
            for (k = 0; k < 2; k++) {
                uint8_t off = k ? p2 : p1;
                uint8_t tx = horizontal ? x0 + off : x0;
                uint8_t ty = horizontal ? y0 : y0 + off;
                e->board[ty][tx] = next;
                emit(e, RT_EV_TRANSMUTE, tx, ty, next);
            }
        } else {
            uint8_t mid = run >> 1;
            uint8_t tx = horizontal ? x0 + mid : x0;
            uint8_t ty = horizontal ? y0 : y0 + mid;
            e->board[ty][tx] = next;
            emit(e, RT_EV_TRANSMUTE, tx, ty, next);
        }
        if (run == 4) try_4chain_bonus(e, type);
    }
}

uint8_t rt_process(rt_engine *e) BANKED
{
#define saved rp_saved
    uint8_t x, y, type, run;
    uint8_t count = 0;

    memcpy(saved, e->board, sizeof(saved));

    emit(e, RT_EV_PASS_BEGIN, e->last_max_run, rt_dirty_rows(e), 0);
    {
        const uint8_t *m = &e->matched[0][0];
        for (y = 0; y < RT_H; y++) {
            uint8_t mask = 0, xbit = 1;
            for (x = 0; x < RT_W; x++, xbit <<= 1, m++)
                if (*m) mask |= xbit;
            if (mask) emit(e, RT_EV_MATCH_ROW, y, mask, 0);
        }
    }

    {
        const uint8_t *m = &e->matched[0][0];
        uint8_t *b = &e->board[0][0];
        uint8_t i;
        for (i = 0; i < RT_H * RT_W; i++, m++, b++)
            if (*m) {
                uint8_t k = *b;
                *b = RT_EMPTY;
                e->cleared_total++;   /* lifetime clear count */
                if (e->cleared_by_kind[k] != 255)
                    e->cleared_by_kind[k]++;
            }
    }

    RT_YIELD(e);
    flood_manna(e, saved);
    RT_YIELD(e);

    for (y = 0; y < RT_H; y++) {
        const uint8_t *mrow = e->matched[y];
        const uint8_t *srow = saved[y];
        x = 0;
        while (x < RT_W) {
            uint8_t i;
            if (!mrow[x]) { x++; continue; }
            type = srow[x];
            i = x;
            while (i < RT_W && mrow[i] && srow[i] == type) i++;
            run = i; run -= x;
            if (run >= 3) {
                if (type >= RT_METAL_FIRST && type <= RT_METAL_LAST)
                    metal_run(e, type, run, x, y, 1);
                count++;
            }
            x = i;
        }
    }

    RT_YIELD(e);
    for (x = 0; x < RT_W; x++) {
        const uint8_t *mcol = &e->matched[0][x];   /* stride RT_W */
        const uint8_t *scol = &saved[0][x];
        uint8_t idx = 0;
        y = 0;
        while (y < RT_H) {
            uint8_t j, jdx;
            if (!mcol[idx]) { y++; idx += RT_W; continue; }
            type = scol[idx];
            j = y; jdx = idx;
            while (j < RT_H && mcol[jdx] && scol[jdx] == type) { j++; jdx += RT_W; }
            run = j; run -= y;
            if (run >= 3) {
                if (type >= RT_METAL_FIRST && type <= RT_METAL_LAST)
                    metal_run(e, type, run, x, y, 0);
                count++;
            }
            y = j; idx = jdx;
        }
    }

    emit(e, RT_EV_PASS_END, count, 0, 0);
    return count;
#undef saved
}

/* ── gravity / refill ──────────────────────────────── */

uint8_t rt_gravity(rt_engine *e) BANKED
{
    uint8_t x, y, write_y;
    uint8_t moved = 0;
    for (x = 0; x < RT_W; x++) {
        uint8_t *col = &e->board[0][x];            /* stride RT_W */
        write_y = RT_H - 1;
        for (y = RT_H; y > 0; ) {
            uint8_t t;
            y--;
            t = col[(uint8_t)(y << 3)];
            if (t != RT_EMPTY) {
                if (y != write_y) {
                    col[(uint8_t)(write_y << 3)] = t;
                    col[(uint8_t)(y << 3)] = RT_EMPTY;
                    emit(e, RT_EV_FALL, x, y, write_y);
                    moved = 1;
                }
                write_y--;
            }
        }
    }
    return moved;
}

void rt_refill(rt_engine *e, uint8_t (*tile_for)(rt_engine *e,
                                                 uint8_t x,
                                                 uint8_t y)) BANKED
{
    /* Column-major, matching legacy board_refill() — the iteration
       order is observable through the RNG stream a game's tile_for
       consumes, so it is part of the behavioral contract. */
    uint8_t x, y;
    for (x = 0; x < RT_W; x++) {
        for (y = 0; y < RT_H; y++) {
            if (e->board[y][x] == RT_EMPTY) {
                uint8_t t = tile_for(e, x, y);
                e->board[y][x] = t;
                emit(e, RT_EV_REFILL, x, y, t);
            }
        }
    }
}

uint8_t rt_resolve_pass(rt_engine *e) BANKED
{
    uint8_t groups;
    e->event_count = 0;
    if (!rt_find(e)) return 0;
    groups = rt_process(e);
    rt_gravity(e);
    return groups;
}

/* ── legal moves ───────────────────────────────────── */

static uint8_t check_match_at(rt_engine *e, uint8_t x, uint8_t y)
{
    const uint8_t *row = e->board[y];
    const uint8_t *col = &e->board[0][x];      /* stride RT_W */
    uint8_t type = row[x];
    uint8_t count, i, idx, yidx;
    if (type == RT_EMPTY) return 0;

    count = 1;
    i = x;
    while (i > 0 && row[i - 1] == type) { count++; i--; }
    i = x;
    while (i < RT_W - 1 && row[i + 1] == type) { count++; i++; }
    if (count >= 3) return 1;

    count = 1;
    yidx = (uint8_t)(y << 3);
    i = y; idx = yidx;
    while (i > 0 && col[idx - RT_W] == type) { count++; i--; idx -= RT_W; }
    i = y; idx = yidx;
    while (i < RT_H - 1 && col[idx + RT_W] == type) { count++; i++; idx += RT_W; }
    return count >= 3;
}

uint8_t rt_find_hint(rt_engine *e, uint8_t *hx, uint8_t *hy) BANKED
{
    uint8_t x, y, tmp;
    for (y = 0; y < RT_H; y++) {
        for (x = 0; x < RT_W; x++) {
            if (x < RT_W - 1) {
                tmp = e->board[y][x];
                e->board[y][x] = e->board[y][x + 1];
                e->board[y][x + 1] = tmp;
                if (check_match_at(e, x, y) ||
                    check_match_at(e, x + 1, y)) {
                    e->board[y][x + 1] = e->board[y][x];
                    e->board[y][x] = tmp;
                    *hx = x;
                    *hy = y;
                    return 1;
                }
                e->board[y][x + 1] = e->board[y][x];
                e->board[y][x] = tmp;
            }
            if (y < RT_H - 1) {
                tmp = e->board[y][x];
                e->board[y][x] = e->board[y + 1][x];
                e->board[y + 1][x] = tmp;
                if (check_match_at(e, x, y) ||
                    check_match_at(e, x, y + 1)) {
                    e->board[y + 1][x] = e->board[y][x];
                    e->board[y][x] = tmp;
                    *hx = x;
                    *hy = y;
                    return 1;
                }
                e->board[y + 1][x] = e->board[y][x];
                e->board[y][x] = tmp;
            }
        }
    }
    return 0;
}

uint8_t rt_find_hint_swap(rt_engine *e,
                          uint8_t *x1, uint8_t *y1,
                          uint8_t *x2, uint8_t *y2) BANKED
{
    uint8_t x, y, tmp;
    for (y = 0; y < RT_H; y++) {
        for (x = 0; x < RT_W; x++) {
            if (x < RT_W - 1) {
                tmp = e->board[y][x];
                e->board[y][x] = e->board[y][x + 1];
                e->board[y][x + 1] = tmp;
                if (check_match_at(e, x, y) ||
                    check_match_at(e, x + 1, y)) {
                    e->board[y][x + 1] = e->board[y][x];
                    e->board[y][x] = tmp;
                    *x1 = x; *y1 = y; *x2 = x + 1; *y2 = y;
                    return 1;
                }
                e->board[y][x + 1] = e->board[y][x];
                e->board[y][x] = tmp;
            }
            if (y < RT_H - 1) {
                tmp = e->board[y][x];
                e->board[y][x] = e->board[y + 1][x];
                e->board[y + 1][x] = tmp;
                if (check_match_at(e, x, y) ||
                    check_match_at(e, x, y + 1)) {
                    e->board[y + 1][x] = e->board[y][x];
                    e->board[y][x] = tmp;
                    *x1 = x; *y1 = y; *x2 = x; *y2 = y + 1;
                    return 1;
                }
                e->board[y + 1][x] = e->board[y][x];
                e->board[y][x] = tmp;
            }
        }
    }
    return 0;
}

uint8_t rt_count_moves(const rt_engine *e) BANKED
{
    uint8_t x, y, tmp, count = 0;
    rt_engine *w = (rt_engine *)e;   /* swap-and-restore, net zero */

    for (y = 0; y < RT_H; y++) {
        /* the exhaustive count ran ~3.5 frames after every cascade
           (the endless "MV" counter): pump every other row */
        if (y & 1) RT_YIELD(w);
        for (x = 0; x < RT_W; x++) {
            if (w->board[y][x] == RT_EMPTY) continue;
            if (x < RT_W - 1 && w->board[y][x + 1] != RT_EMPTY) {
                tmp = w->board[y][x];
                w->board[y][x] = w->board[y][x + 1];
                w->board[y][x + 1] = tmp;
                if (check_match_at(w, x, y) || check_match_at(w, x + 1, y))
                    count++;
                tmp = w->board[y][x];
                w->board[y][x] = w->board[y][x + 1];
                w->board[y][x + 1] = tmp;
            }
            if (y < RT_H - 1 && w->board[y + 1][x] != RT_EMPTY) {
                tmp = w->board[y][x];
                w->board[y][x] = w->board[y + 1][x];
                w->board[y + 1][x] = tmp;
                if (check_match_at(w, x, y) || check_match_at(w, x, y + 1))
                    count++;
                tmp = w->board[y][x];
                w->board[y][x] = w->board[y + 1][x];
                w->board[y + 1][x] = tmp;
            }
        }
    }
    return count;
}

uint8_t rt_has_legal_moves(rt_engine *e) BANKED
{
    uint8_t x, y, tmp, hit;
    for (y = 0; y < RT_H; y++) {
        if (y & 2) RT_YIELD(e);    /* ~0.45 frame per row under SDCC */
        for (x = 0; x < RT_W; x++) {
            if (x < RT_W - 1) {
                tmp = e->board[y][x];
                e->board[y][x] = e->board[y][x + 1];
                e->board[y][x + 1] = tmp;
                hit = check_match_at(e, x, y) || check_match_at(e, x + 1, y);
                tmp = e->board[y][x];
                e->board[y][x] = e->board[y][x + 1];
                e->board[y][x + 1] = tmp;
                if (hit) return 1;
            }
            if (y < RT_H - 1) {
                tmp = e->board[y][x];
                e->board[y][x] = e->board[y + 1][x];
                e->board[y + 1][x] = tmp;
                hit = check_match_at(e, x, y) || check_match_at(e, x, y + 1);
                tmp = e->board[y][x];
                e->board[y][x] = e->board[y + 1][x];
                e->board[y + 1][x] = tmp;
                if (hit) return 1;
            }
        }
    }
    return 0;
}
