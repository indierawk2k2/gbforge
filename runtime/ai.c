/* gbforge runtime — CPU move search. See ai.h. Exact port of legacy
 * battle_ai.c scoring. */

#ifdef __SDCC
#pragma bank 2
#endif

#include "ai.h"

static uint16_t ai_score_board(const rt_engine *e, uint16_t *tier_bonus)
{
    uint16_t score = 0;
    uint16_t tier = 0;
    uint8_t x, y, run, type;

    for (y = 0; y < RT_H; y++) {
        run = 1;
        for (x = 1; x < RT_W; x++) {
            if (e->board[y][x] != RT_EMPTY &&
                e->board[y][x] == e->board[y][x - 1]) {
                run++;
            } else {
                if (run >= 3) {
                    type = e->board[y][x - 1];
                    score += (run >= 4) ? (uint16_t)run * 3 : (uint16_t)run;
                    tier += rt_knowledge_for_tier(type) * (uint16_t)run;
                }
                run = 1;
            }
        }
        if (run >= 3) {
            type = e->board[y][RT_W - 1];
            score += (run >= 4) ? (uint16_t)run * 3 : (uint16_t)run;
            tier += rt_knowledge_for_tier(type) * (uint16_t)run;
        }
    }

    for (x = 0; x < RT_W; x++) {
        run = 1;
        for (y = 1; y < RT_H; y++) {
            if (e->board[y][x] != RT_EMPTY &&
                e->board[y][x] == e->board[y - 1][x]) {
                run++;
            } else {
                if (run >= 3) {
                    type = e->board[y - 1][x];
                    score += (run >= 4) ? (uint16_t)run * 3 : (uint16_t)run;
                    tier += rt_knowledge_for_tier(type) * (uint16_t)run;
                }
                run = 1;
            }
        }
        if (run >= 3) {
            type = e->board[RT_H - 1][x];
            score += (run >= 4) ? (uint16_t)run * 3 : (uint16_t)run;
            tier += rt_knowledge_for_tier(type) * (uint16_t)run;
        }
    }

    *tier_bonus = tier;
    return score;
}

static uint16_t ai_evaluate_swap(rt_engine *e,
                                 uint8_t x1, uint8_t y1,
                                 uint8_t x2, uint8_t y2,
                                 uint16_t *tier)
{
    uint16_t score;
    uint8_t tmp = e->board[y1][x1];
    e->board[y1][x1] = e->board[y2][x2];
    e->board[y2][x2] = tmp;
    score = ai_score_board(e, tier);
    tmp = e->board[y1][x1];
    e->board[y1][x1] = e->board[y2][x2];
    e->board[y2][x2] = tmp;
    return score;
}

uint8_t rt_ai_best_swap(rt_engine *e,
                        uint8_t *best_x1, uint8_t *best_y1,
                        uint8_t *best_x2, uint8_t *best_y2) BANKED
{
    uint16_t best_score = 0;
    uint16_t best_tier = 0;
    uint8_t found = 0;
    uint8_t x, y;
    uint16_t score, tier;

    for (y = 0; y < RT_H; y++) {
        for (x = 0; x < RT_W; x++) {
            if (x < RT_W - 1) {
                score = ai_evaluate_swap(e, x, y, x + 1, y, &tier);
                if (score > best_score ||
                    (score == best_score && score > 0 &&
                     tier > best_tier)) {
                    best_score = score;
                    best_tier = tier;
                    *best_x1 = x;
                    *best_y1 = y;
                    *best_x2 = x + 1;
                    *best_y2 = y;
                    found = 1;
                }
            }
            if (y < RT_H - 1) {
                score = ai_evaluate_swap(e, x, y, x, y + 1, &tier);
                if (score > best_score ||
                    (score == best_score && score > 0 &&
                     tier > best_tier)) {
                    best_score = score;
                    best_tier = tier;
                    *best_x1 = x;
                    *best_y1 = y;
                    *best_x2 = x;
                    *best_y2 = y + 1;
                    found = 1;
                }
            }
        }
    }
    return found;
}
