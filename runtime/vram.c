/* gbforge runtime — VRAM primitives. See vram.h. */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>

#include "vram.h"

/* Editor-owned res/ contracts (asset boundary — see gbforge/model/assets.py) */
#include "tiles_data.h"
#include "palettes.h"
#include "sprites_data.h"
#include "title_logo.h"

#define BOARD_OFFSET 1  /* board origin in bkg tile coords (x and y) */

/* Horizontal sprite origin of the board. 3 under the default
   SCX=5 layout; puzzle mode centers the board (SCX=248) and shifts
   board-anchored sprites +8 (legacy screen_off_x). BG writes are
   unaffected — only sprites live in screen space. */
uint8_t rt_screen_off_x = 3;   /* RT_SCREEN_OFF_X (hud.h) */

/* Just the cursor bracket sprites — for screens that build their
   own BG (tutorials) but want the real cursor without reloading
   the whole game tile set over their art. */
void rtv_load_cursor_sprites(void)
{
    uint8_t saved = _current_bank;
    SWITCH_ROM(2);
    set_sprite_data(0, CURSOR_TOTAL_TILES, cursor_sprite_data);
    SWITCH_ROM(saved);
}

void rtv_load_game_gfx(void)
{
    uint8_t saved = _current_bank;
    SWITCH_ROM(2);  /* res gfx bank (pinned editor contract) */
    set_bkg_data(0, TOTAL_GAME_HW_TILES, tile_data);
    set_bkg_data(UI_TILE_BASE, UI_TILE_COUNT, ui_tile_data);
    set_bkg_data(SPELL_ICON_TILE_BASE, SPELL_ICON_COUNT,
                 spell_icon_tile_data);
    set_bkg_data(GHOST_TILE_BASE, TOTAL_GAME_HW_TILES,
                 ghost_tile_data);   /* refill drop-in transit tiles */
    set_sprite_data(0, CURSOR_TOTAL_TILES, cursor_sprite_data);
    set_sprite_data(KNOWLEDGE_TILE_BASE, KNOWLEDGE_TOTAL_TILES,
                    knowledge_sprite_data);
    set_sprite_data(BURST_TILE_BASE, BURST_TOTAL_TILES, burst_sprite_data);
    set_sprite_data(FLOAT_TILE_PLUS, FLOAT_TOTAL_TILES, float_sprite_data);
    set_sprite_data(MOVES_SPR_TILE_M, 12, moves_sprite_data);
    set_sprite_data(OPP_KNOWLEDGE_TILE_C, OPP_KNOWLEDGE_TOTAL_TILES,
                    opp_knowledge_sprite_data);
    SWITCH_ROM(saved);

    set_bkg_palette(0, 8, bg_palettes);
    set_sprite_palette(0, 4, sprite_palettes);
}

void rtv_load_title_gfx(void)
{
    /* HOME-resident on purpose: SWITCH_ROM under banked code
       would swap out the very bank being executed (the hazard
       legacy render_load.c existed for). */
    uint8_t saved = _current_bank;
    SWITCH_ROM(2);
    set_bkg_data(0, TOTAL_GAME_HW_TILES, tile_data);
    set_bkg_data(UI_TILE_BASE, UI_TILE_COUNT, ui_tile_data);
    /* The logo is baked by gen_title from the spec's text, not a res
       asset — rtt_show uploads it itself. */
    SWITCH_ROM(saved);
}

void rtv_load_hint_sprite(uint8_t tile_type)
{
    uint8_t saved = _current_bank;
    SWITCH_ROM(2);
    set_sprite_data(HINT_TILE_BASE, 4,
                    &tile_data[HW_TILE_BASE(tile_type) * 16]);
    SWITCH_ROM(saved);
}

void rtv_load_swap_sprites(uint8_t type_a, uint8_t type_b)
{
    uint8_t saved = _current_bank;
    SWITCH_ROM(2);
    set_sprite_data(SWAP_TILE_A, 4, &tile_data[HW_TILE_BASE(type_a) * 16]);
    set_sprite_data(SWAP_TILE_B, 4, &tile_data[HW_TILE_BASE(type_b) * 16]);
    SWITCH_ROM(saved);
    /* OBJ palettes mirror the tiles' BG palettes (legacy technique) */
    set_sprite_palette(SWAP_PAL_A, 1,
                       &bg_palettes[tile_palette_map[type_a][0] * 4]);
    set_sprite_palette(SWAP_PAL_B, 1,
                       &bg_palettes[tile_palette_map[type_b][0] * 4]);
}

/* The staged blast / DMA shadow / board blits live in vram_dma.c
   (bank 3): HOME is nearly full and they need no bank switching. */
