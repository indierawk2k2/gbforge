/* Cascadia — per-game glue over the gbforge runtime.
 *
 * Everything with a tuning knob lives in cascadia.py and arrives
 * here through the generated tables; this file is the wiring: input
 * edges, the score/move counters, and the win/lose ladder. A future
 * gen_main emits this file too.
 *
 * Rules (all visible below): 4 tile kinds, swaps commit only when
 * they match, 10 points per cleared tile, cascades score every
 * stage, win at 1000 points, lose when 30 moves run out.
 */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <rand.h>
#include <string.h>

#include "engine.h"
#include "anim.h"
#include "flow.h"
#include "vram.h"
#include "hud.h"
#include "ui_core.h"
#include "cursor.h"
#include "reset.h"
#include "title.h"
#include "tiles_data.h"
#include "debug.h"
#include "mode_config.h"
#include "ui_overlays.h"
#include "title_config.h"

#define TILE_KINDS   4
#define POINTS_PER_TILE 10
#define WIN_SCORE    1000
#define START_MOVES  30

static rt_engine eng;

/* harness-readable mirrors */
uint8_t board[8][8];
uint8_t cursor_x = 0, cursor_y = 0;
uint8_t tile_selected = 0;
uint8_t processing_matches = 0;
uint16_t score = 0;
uint8_t moves_left = 0;

static uint8_t prev_keys = 0;
static uint8_t frame_counter = 0;

static void seed_rng(void)
{
    uint16_t seed;
#ifdef DEBUG_BUILD
    if (debug_rng_force) {
        initrand(debug_rng_seed);
        return;
    }
#endif
    seed = DIV_REG;
    seed = (seed << 8) | DIV_REG;
    initrand(seed);
}

static uint8_t random_tile(void)
{
    /* uniform over the four gems (rand() is signed: cast first) */
    uint8_t r;
    do {
        r = ((uint8_t)rand()) & 0x07;
    } while (r >= TILE_KINDS * 2);
    return (r >> 1) + 1;
}

static uint8_t would_match(uint8_t x, uint8_t y, uint8_t t)
{
    if (x >= 2 && eng.board[y][x-1] == t && eng.board[y][x-2] == t) return 1;
    if (y >= 2 && eng.board[y-1][x] == t && eng.board[y-2][x] == t) return 1;
    return 0;
}

static uint8_t refill_tile(rt_engine *e, uint8_t x, uint8_t y)
{
    uint8_t tile, attempts = 0;
    (void)e;
    do {
        tile = random_tile();
        attempts++;
    } while (would_match(x, y, tile) && attempts < 20);
    return tile;
}

static uint8_t coord3(void)
{
    return ((uint8_t)rand()) & 0x07;
}

static uint8_t cx_rng(uint8_t n)
{
    return (uint8_t)(rand() % n);
}

/* The starting board never contains a match (refill_tile rerolls),
   and never starts dead (reroll the whole board if move-less). */
static void board_fill(void)
{
    uint8_t x, y;
    seed_rng();
    do {
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++)
                eng.board[y][x] = refill_tile(&eng, x, y);
    } while (!rt_has_legal_moves(&eng));
}

static void sync_state(void)
{
    memcpy(board, eng.board, 64);
    score = eng.cleared_total * POINTS_PER_TILE;
    rth_update(0, 0, 0, score);
    rth_show_moves(moves_left);
}

static void overlay_wait(const ng_overlay *o)
{
    uint8_t keys, pressed;
    uint8_t i;

    for (i = 0; i < 4; i++) move_sprite(i, 0, 0);
    ui_show_overlay(o);
    prev_keys = joypad();
    while (1) {
        vsync();
        keys = joypad();
        pressed = keys & ~prev_keys;
        prev_keys = keys;
        if (pressed & (J_A | J_START)) break;
#ifdef DEBUG_BUILD
        if (debug_req != DBG_NONE) break;
#endif
    }
    rtv_blit_board(eng.board);
    rtc_invalidate();
}

/* One committed swap: resolve, score, then run the win/lose ladder.
   Returns 1 when the round ended. */
static uint8_t do_swap(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    uint8_t passes;

    processing_matches = 1;
    passes = flow_swap(&eng, &ng_modes[0], x1, y1, x2, y2, refill_tile);
    sync_state();
    processing_matches = 0;
    if (!passes) return 0;              /* reverted: move not spent */

    moves_left--;
    sync_state();

    if (score >= WIN_SCORE) {
        overlay_wait(&ovl_win);
        return 1;
    }
    if (moves_left == 0) {
        overlay_wait(&ovl_lose);
        return 1;
    }
    if (!rt_has_legal_moves(&eng)) {
        rta_play_warning(eng.board);
        rt_punitive_reset(&eng, refill_tile, coord3);
        rta_sync(eng.board);
        rtv_blit_board(eng.board);
        sync_state();
    }
    return 0;
}

static void classic_run(void)
{
    uint8_t keys, pressed;

    DISPLAY_OFF;
    rtv_load_game_gfx();
    rth_init();
    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;

    memset(&eng.manna, 0, sizeof(eng.manna));
    eng.knowledge = 0;
    eng.cleared_total = 0;
    eng.max_tier = TILE_KINDS;      /* gems never transmute */
    eng.fx.chain_bonus_pct = 0;
    eng.rng = cx_rng;
    score = 0;
    moves_left = START_MOVES;
    cursor_x = cursor_y = 0;
    tile_selected = 0;
    board_fill();
    rta_sync(eng.board);
    rtv_blit_board(eng.board);
    sync_state();
    rtc_snap(cursor_x, cursor_y);
    rtc_invalidate();
    prev_keys = joypad();

    while (1) {
        vsync();
        frame_counter++;
        rta_apply_shake();
        rta_update_floats();

#ifdef DEBUG_BUILD
        if (debug_req == DBG_TRIGGER_SWAP) {
            uint8_t x = debug_arg0, y = debug_arg1, d = debug_arg2;
            uint8_t x2 = x, y2 = y;
            debug_req = DBG_NONE;
            if (d == 0) y2--; else if (d == 1) y2++;
            else if (d == 2) x2--; else x2++;
            if (do_swap(x, y, x2, y2)) return;
            continue;
        }
        if (debug_req == DBG_REDRAW) {
            debug_req = DBG_NONE;
            memcpy(eng.board, board, 64);
            rta_sync(eng.board);
            rtv_blit_board(eng.board);
            continue;
        }
        if (debug_req != DBG_NONE) debug_req = DBG_NONE;
#endif

        keys = joypad();
        pressed = keys & ~prev_keys;
        prev_keys = keys;

        if (tile_selected) {
            if (pressed & (J_A | J_B)) {
                tile_selected = 0;
            } else if (pressed & (J_UP | J_DOWN | J_LEFT | J_RIGHT)) {
                uint8_t x2 = cursor_x, y2 = cursor_y;
                if ((pressed & J_UP) && cursor_y > 0) y2--;
                if ((pressed & J_DOWN) && cursor_y < 7) y2++;
                if ((pressed & J_LEFT) && cursor_x > 0) x2--;
                if ((pressed & J_RIGHT) && cursor_x < 7) x2++;
                if (x2 != cursor_x || y2 != cursor_y) {
                    tile_selected = 0;
                    if (do_swap(cursor_x, cursor_y, x2, y2)) return;
                }
            }
        } else {
            if (pressed & J_A) tile_selected = 1;
            if ((pressed & J_UP) && cursor_y > 0) cursor_y--;
            if ((pressed & J_DOWN) && cursor_y < 7) cursor_y++;
            if ((pressed & J_LEFT) && cursor_x > 0) cursor_x--;
            if ((pressed & J_RIGHT) && cursor_x < 7) cursor_x++;
        }
        rtc_animate(cursor_x, cursor_y);
        rtc_draw(frame_counter, tile_selected ? 1 : 0);
    }
}

void main(void)
{
    rta_init(eng.board);
    cpu_fast();

    while (1) {
#ifdef DEBUG_BUILD
        if (debug_req == DBG_ENTER_MODE) {
            debug_req = DBG_NONE;
            classic_run();
            continue;
        }
        if (debug_req != DBG_NONE) debug_req = DBG_NONE;
#endif
        rtt_show(&ng_title_screen);
        classic_run();
    }
}
