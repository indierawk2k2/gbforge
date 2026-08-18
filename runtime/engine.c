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

#define MANNA_CAP_DEFAULT 99
#define KNOWLEDGE_CAP 9999u

static const uint16_t knowledge_per_tier[RT_TILE_TYPES] = {
    0, 0, 0, 0, 1, 3, 10, 25, 50, 100, 200, 500
};

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
    return knowledge_per_tier[tile];
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
    if (run >= 6) return 10;
    if (run >= 5) return 7;
    if (run >= 4) return 5;
    return run;
}

static void add_manna(rt_engine *e, uint8_t tile, uint8_t amount)
{
    uint8_t cap = e->fx.manna_cap ? e->fx.manna_cap : MANNA_CAP_DEFAULT;
    uint8_t idx = tile - RT_MANNA_FIRST;
    if (tile < RT_MANNA_FIRST || tile > RT_MANNA_LAST) return;
    if ((uint16_t)e->manna[idx] + amount > cap) e->manna[idx] = cap;
    else e->manna[idx] += amount;
}

static void add_knowledge(rt_engine *e, uint16_t amount)
{
    if (e->knowledge + amount > KNOWLEDGE_CAP) e->knowledge = KNOWLEDGE_CAP;
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
        uint16_t kp = knowledge_per_tier[tile] * run;
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

uint8_t rt_find(rt_engine *e) BANKED
{
    uint8_t x, y, type, run, i;
    uint8_t found = 0;

    memset(e->matched, 0, sizeof(e->matched));
    e->last_max_run = 0;

    for (y = 0; y < RT_H; y++) {
        x = 0;
        while (x < RT_W) {
            type = e->board[y][x];
            if (type == RT_EMPTY) { x++; continue; }
            run = 1;
            while (x + run < RT_W && e->board[y][x + run] == type) run++;
            if (run >= 3) {
                for (i = 0; i < run; i++) e->matched[y][x + i] = 1;
                found = 1;
                if (run > e->last_max_run) e->last_max_run = run;
            }
            x += run;
        }
    }

    for (x = 0; x < RT_W; x++) {
        y = 0;
        while (y < RT_H) {
            type = e->board[y][x];
            if (type == RT_EMPTY) { y++; continue; }
            run = 1;
            while (y + run < RT_H && e->board[y + run][x] == type) run++;
            if (run >= 3) {
                for (i = 0; i < run; i++) e->matched[y + i][x] = 1;
                found = 1;
                if (run > e->last_max_run) e->last_max_run = run;
            }
            y += run;
        }
    }

    if (found) {
        uint8_t counts[RT_TILE_TYPES];
        memset(counts, 0, sizeof(counts));
        for (y = 0; y < RT_H; y++)
            for (x = 0; x < RT_W; x++)
                if (e->matched[y][x]) counts[e->board[y][x]]++;
        for (type = 1; type < RT_TILE_TYPES; type++)
            if (counts[type] > e->last_max_run)
                e->last_max_run = counts[type];
    }
    return found;
}

uint8_t rt_dirty_rows(const rt_engine *e) BANKED
{
    uint8_t x, y, dirty = 0;
    for (y = 0; y < RT_H; y++) {
        for (x = 0; x < RT_W; x++) {
            if (e->matched[y][x]) { dirty |= (uint8_t)(1 << y); break; }
        }
    }
    return dirty;
}

/* ── process ───────────────────────────────────────── */

static void flood_manna(rt_engine *e, const uint8_t saved[RT_H][RT_W])
{
    uint8_t awarded[RT_H][RT_W];
    uint8_t stack_x[RT_H * RT_W], stack_y[RT_H * RT_W];
    uint8_t sx, sy, x, y, type, sp, count, sum_x, sum_y;

    memset(awarded, 0, sizeof(awarded));

    for (sy = 0; sy < RT_H; sy++) {
        for (sx = 0; sx < RT_W; sx++) {
            if (!e->matched[sy][sx] || awarded[sy][sx]) continue;
            type = saved[sy][sx];
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
}

static void try_4chain_bonus(rt_engine *e, uint8_t metal_type)
{
    uint8_t next = rt_next_tier(e, metal_type);
    uint8_t cand_x[RT_H * RT_W], cand_y[RT_H * RT_W];
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
    uint8_t saved[RT_H][RT_W];
    uint8_t x, y, type, run;
    uint8_t count = 0;

    memcpy(saved, e->board, sizeof(saved));

    emit(e, RT_EV_PASS_BEGIN, e->last_max_run, rt_dirty_rows(e), 0);
    for (y = 0; y < RT_H; y++) {
        uint8_t mask = 0;
        for (x = 0; x < RT_W; x++)
            if (e->matched[y][x]) mask |= (uint8_t)(1 << x);
        if (mask) emit(e, RT_EV_MATCH_ROW, y, mask, 0);
    }

    for (y = 0; y < RT_H; y++)
        for (x = 0; x < RT_W; x++)
            if (e->matched[y][x]) {
                uint8_t k = e->board[y][x];
                e->board[y][x] = RT_EMPTY;
                e->cleared_total++;   /* lifetime clear count */
                if (e->cleared_by_kind[k] != 255)
                    e->cleared_by_kind[k]++;
            }

    flood_manna(e, saved);

    for (y = 0; y < RT_H; y++) {
        x = 0;
        while (x < RT_W) {
            if (!e->matched[y][x]) { x++; continue; }
            type = saved[y][x];
            run = 0;
            while (x + run < RT_W && e->matched[y][x + run] &&
                   saved[y][x + run] == type) run++;
            if (run >= 3) {
                if (type >= RT_METAL_FIRST && type <= RT_METAL_LAST)
                    metal_run(e, type, run, x, y, 1);
                count++;
            }
            x += run;
        }
    }

    for (x = 0; x < RT_W; x++) {
        y = 0;
        while (y < RT_H) {
            if (!e->matched[y][x]) { y++; continue; }
            type = saved[y][x];
            run = 0;
            while (y + run < RT_H && e->matched[y + run][x] &&
                   saved[y + run][x] == type) run++;
            if (run >= 3) {
                if (type >= RT_METAL_FIRST && type <= RT_METAL_LAST)
                    metal_run(e, type, run, x, y, 0);
                count++;
            }
            y += run;
        }
    }

    emit(e, RT_EV_PASS_END, count, 0, 0);
    return count;
}

/* ── gravity / refill ──────────────────────────────── */

uint8_t rt_gravity(rt_engine *e) BANKED
{
    uint8_t x, y, write_y;
    uint8_t moved = 0;
    for (x = 0; x < RT_W; x++) {
        write_y = RT_H - 1;
        for (y = RT_H; y > 0; ) {
            y--;
            if (e->board[y][x] != RT_EMPTY) {
                if (y != write_y) {
                    e->board[write_y][x] = e->board[y][x];
                    e->board[y][x] = RT_EMPTY;
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
    uint8_t type = e->board[y][x];
    uint8_t count, i;
    if (type == RT_EMPTY) return 0;

    count = 1;
    i = x;
    while (i > 0 && e->board[y][i - 1] == type) { count++; i--; }
    i = x;
    while (i < RT_W - 1 && e->board[y][i + 1] == type) { count++; i++; }
    if (count >= 3) return 1;

    count = 1;
    i = y;
    while (i > 0 && e->board[i - 1][x] == type) { count++; i--; }
    i = y;
    while (i < RT_H - 1 && e->board[i + 1][x] == type) { count++; i++; }
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
