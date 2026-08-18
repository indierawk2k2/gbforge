/* gbforge runtime — ability interpreter. See ability.h. Effect
 * semantics are the legacy cast_spell() behaviors, table-driven. */

#ifdef __SDCC
#pragma bank 3   /* bank 1 is the engine+animator's; casts are cold */
#endif

#include "ability.h"

uint8_t ab_can_cast(const ng_ability *a, const rt_engine *e) BANKED
{
    return e->manna[0] >= a->cost_fire &&
           e->manna[1] >= a->cost_water &&
           e->manna[2] >= a->cost_earth;
}

uint8_t ab_target_ok(const ng_ability *a, const rt_engine *e,
                     uint8_t tx, uint8_t ty) BANKED
{
    uint8_t t;
    if (a->targeting == AB_TARGET_NONE) return 1;
    t = e->board[ty][tx];
    switch (a->target_filter) {
        case AB_FILTER_ANY_NONEMPTY:
            return t != RT_EMPTY;
        case AB_FILTER_MANNA:
            return t >= RT_MANNA_FIRST && t <= RT_MANNA_LAST;
        case AB_FILTER_METAL_PROMOTABLE:
            return t >= RT_METAL_FIRST && t < RT_METAL_LAST;
        default:
            return 1;
    }
}

static void deduct(const ng_ability *a, rt_engine *e)
{
    e->manna[0] -= a->cost_fire;
    e->manna[1] -= a->cost_water;
    e->manna[2] -= a->cost_earth;
}

uint8_t ab_cast(const ng_ability *a, rt_engine *e,
                uint8_t tx, uint8_t ty) BANKED
{
    uint8_t x, y;

    if (!ab_can_cast(a, e)) return 0;
    if (!ab_target_ok(a, e, tx, ty)) return 0;

    switch (a->op) {
        case AB_OP_DESTROY_TILE:
            deduct(a, e);
            e->board[ty][tx] = RT_EMPTY;
            return 1;

        case AB_OP_DESTROY_ROW:
            deduct(a, e);
            for (x = 0; x < RT_W; x++) e->board[ty][x] = RT_EMPTY;
            return 1;

        case AB_OP_DESTROY_TYPE: {
            uint8_t type = e->board[ty][tx];
            deduct(a, e);
            for (y = 0; y < RT_H; y++)
                for (x = 0; x < RT_W; x++)
                    if (e->board[y][x] == type)
                        e->board[y][x] = RT_EMPTY;
            return 1;
        }

        case AB_OP_CONVERT_TILE:
            deduct(a, e);
            e->board[ty][tx] = a->op_arg;
            return 1;

        case AB_OP_PROMOTE_TILE:
            deduct(a, e);
            e->board[ty][tx]++;
            return 1;

        case AB_OP_SHUFFLE_MANNA: {
            /* legacy Tectonic: Fisher-Yates over manna tile TYPES,
               positions stay (metals anchored). rand()%(i+1) order. */
            uint8_t px[RT_H * RT_W], py[RT_H * RT_W], pt[RT_H * RT_W];
            uint8_t n = 0, i, j, tmp;
            if (e->rng == 0) return 0;
            deduct(a, e);
            for (y = 0; y < RT_H; y++)
                for (x = 0; x < RT_W; x++) {
                    uint8_t t = e->board[y][x];
                    if (t >= RT_MANNA_FIRST && t <= RT_MANNA_LAST) {
                        px[n] = x;
                        py[n] = y;
                        pt[n] = t;
                        n++;
                    }
                }
            for (i = n; i > 1; ) {
                i--;
                j = e->rng(i + 1);
                tmp = pt[i];
                pt[i] = pt[j];
                pt[j] = tmp;
            }
            for (i = 0; i < n; i++) e->board[py[i]][px[i]] = pt[i];
            return 1;
        }

        case AB_OP_FREE_SWAP:
            /* cost only — the caller runs the unconditional swap */
            deduct(a, e);
            return 1;

        default:
            return 0;
    }
}
