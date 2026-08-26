#ifndef GBFORGE_VRAM_H_INCLUDED
#define GBFORGE_VRAM_H_INCLUDED

/* gbforge runtime — VRAM primitives for the standard board layout:
 * 8x8 logical tiles, each 2x2 hardware tiles, board origin at bkg
 * tile (1,1). Tile graphics and palette maps are the editor-owned
 * res/ assets (tiles_data / palettes), loaded from their pinned bank.
 */

#include <stdint.h>

/* Under GBDK/SDCC BANKED must be the real __banked keyword (see
   engine.h) — the staged-blit group below is banked (vram_dma.c). */
#ifdef __SDCC
#include <asm/types.h>
#else
#ifndef BANKED
#define BANKED
#endif
#endif

/* Load game tile graphics + palettes (res/ assets, bank 2). */
extern uint8_t rt_screen_off_x;   /* board sprite origin (3 or 11) */

void rtv_load_game_gfx(void);
void rtv_load_cursor_sprites(void);

/* Title gfx (game tiles + UI + logo). HOME-resident bank trampoline. */
void rtv_load_title_gfx(void);

/* Blit the whole board: stages all 8 rows, waits for vblank itself,
 * then blasts. Callers must NOT rely on a preceding vsync (a redundant
 * one only costs a frame). Safe with the LCD off. */
void rtv_blit_board(const uint8_t board[8][8]) BANKED;

/* Staged blast: rows are precomputed CPU-side (32 attr + 32 tile
 * bytes per logical row = two hw map rows), then landed with CGB
 * general-purpose DMA — a whole board in ~5 scanlines of vblank, with
 * an HBlank-synced fallback for anything that outruns it. Stage
 * BEFORE vsync (staging is slow), blast right after it. Up to 8 rows
 * (16 hw rows) per blast. */
void rtv_stage_reset(void) BANKED;
void rtv_stage_row(uint8_t board_row,
                   const uint8_t attr[32], const uint8_t tile[32]) BANKED;
void rtv_blast(void) BANKED;

/* Stage one board row rendered normally from `board`. */
void rtv_stage_board_row(const uint8_t board[8][8], uint8_t y) BANKED;

/* Blit a single logical tile (2x2 hw tiles + attrs) straight to VRAM
 * (call right after vsync — it is not beam-synchronised). */
void rtv_blit_tile(uint8_t bx, uint8_t by, uint8_t type) BANKED;

/* Stage a single logical tile for the next rtv_blast (beam-safe). */
void rtv_stage_tile(uint8_t bx, uint8_t by, uint8_t type) BANKED;

/* Stage one cell's palette attr only (match flash). */
void rtv_stage_cell_attr(uint8_t bx, uint8_t by, uint8_t attr) BANKED;

/* In-shadow motion for the fall / refill animation (see vram_dma.c):
 * move a column's falling cells' hw rows down one row (phase 0 = half
 * step, 1 = full step), or run the refill conveyor one step for a
 * column. Land with rtv_blast. */
void rtv_shift_column(uint8_t bx, uint8_t mask, uint8_t phase) BANKED;
void rtv_conveyor_step(uint8_t bx, uint8_t k,
                       uint8_t t0, uint8_t t1, uint8_t attr) BANKED;

/* Load two tile types' gfx into the borrowed swap sprite tiles and
 * mirror their BG palettes into the swap OBJ palette slots. */
void rtv_load_swap_sprites(uint8_t type_a, uint8_t type_b);

/* Load one tile's gfx into the hint sprite tiles (HOME trampoline). */
void rtv_load_hint_sprite(uint8_t tile_type);

#endif
