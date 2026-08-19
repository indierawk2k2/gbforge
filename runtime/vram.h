#ifndef GBFORGE_VRAM_H_INCLUDED
#define GBFORGE_VRAM_H_INCLUDED

/* gbforge runtime — VRAM primitives for the standard board layout:
 * 8x8 logical tiles, each 2x2 hardware tiles, board origin at bkg
 * tile (1,1). Tile graphics and palette maps are the editor-owned
 * res/ assets (tiles_data / palettes), loaded from their pinned bank.
 */

#include <stdint.h>

#ifndef BANKED
#define BANKED
#endif

/* Load game tile graphics + palettes (res/ assets, bank 2). */
extern uint8_t rt_screen_off_x;   /* board sprite origin (3 or 11) */

void rtv_load_game_gfx(void);

/* Title gfx (game tiles + UI + logo). HOME-resident bank trampoline. */
void rtv_load_title_gfx(void);

/* Blit the whole board: tile indices and CGB palette attributes,
 * batched per VBK bank to minimize switches. */
void rtv_blit_board(const uint8_t board[8][8]);

/* Staged blast (legacy render_vram_blast_interleaved port): rows are
 * precomputed CPU-side (32 attr + 32 tile bytes per logical row =
 * two hw map rows), then written raw during vblank or STAT-guarded
 * during active display. This is what keeps mid-animation updates
 * tear-free in a single frame. */
void rtv_stage_reset(void);
void rtv_stage_row(uint8_t board_row,
                   const uint8_t attr[32], const uint8_t tile[32]);
void rtv_blast(void);

/* Stage one board row rendered normally from `board`. */
void rtv_stage_board_row(const uint8_t board[8][8], uint8_t y);

/* Blit a single logical tile (2x2 hw tiles + attrs). */
void rtv_blit_tile(uint8_t bx, uint8_t by, uint8_t type);

/* Load two tile types' gfx into the borrowed swap sprite tiles and
 * mirror their BG palettes into the swap OBJ palette slots. */
void rtv_load_swap_sprites(uint8_t type_a, uint8_t type_b);

/* Load one tile's gfx into the hint sprite tiles (HOME trampoline). */
void rtv_load_hint_sprite(uint8_t tile_type);

#endif
