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
    set_bkg_data(TITLE_LOGO_TILE_BASE, TITLE_LOGO_TILE_COUNT,
                 title_logo_tiles);
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

void rtv_blit_tile(uint8_t bx, uint8_t by, uint8_t type)
{
    uint8_t tiles[4];
    uint8_t attrs[4];
    uint8_t base = HW_TILE_BASE(type);
    uint8_t i;
    uint8_t tx = BOARD_OFFSET + (bx << 1);
    uint8_t ty = BOARD_OFFSET + (by << 1);

    for (i = 0; i < 4; i++) {
        tiles[i] = base + i;
        attrs[i] = tile_palette_map[type][i];
    }

    VBK_REG = VBK_ATTRIBUTES;
    set_bkg_tiles(tx, ty, 2, 2, attrs);
    VBK_REG = VBK_TILES;
    set_bkg_tiles(tx, ty, 2, 2, tiles);
}

/* ── staged interleaved blast (legacy render.c port) ─────────────
 * Rows are staged CPU-side, then written attr+tile together per hw
 * row so the LCD never scans mismatched pairs. In vblank: raw
 * copies; during display: STAT-guarded per byte. */

#define STAGE_MAX_ROWS 8

static uint8_t stage_attr[STAGE_MAX_ROWS][32];
static uint8_t stage_tile[STAGE_MAX_ROWS][32];
static uint8_t stage_map_y[STAGE_MAX_ROWS];
static uint8_t stage_count;

static void stat_safe_copy(uint8_t *dest, const uint8_t *src, uint8_t len)
{
    while (len--) {
        while (STAT_REG & 0x02);  /* spin until not PPU mode 2/3 */
        *dest++ = *src++;
    }
}

void rtv_stage_reset(void)
{
    stage_count = 0;
}

void rtv_stage_row(uint8_t board_row,
                   const uint8_t attr[32], const uint8_t tile[32])
{
    uint8_t i = stage_count;
    if (i >= STAGE_MAX_ROWS) return;
    memcpy(stage_attr[i], attr, 32);
    memcpy(stage_tile[i], tile, 32);
    stage_map_y[i] = (board_row << 1) + BOARD_OFFSET;
    stage_count = i + 1;
}

void rtv_stage_board_row(const uint8_t board[8][8], uint8_t y)
{
    uint8_t attr[32], tile[32];
    uint8_t x;
    for (x = 0; x < 8; x++) {
        uint8_t t = board[y][x];
        uint8_t base = HW_TILE_BASE(t);
        const uint8_t *pal = tile_palette_map[t];
        tile[(x << 1)] = base;
        tile[(x << 1) + 1] = base + 1;
        tile[16 + (x << 1)] = base + 2;
        tile[16 + (x << 1) + 1] = base + 3;
        attr[(x << 1)] = pal[0];
        attr[(x << 1) + 1] = pal[1];
        attr[16 + (x << 1)] = pal[2];
        attr[16 + (x << 1) + 1] = pal[3];
    }
    rtv_stage_row(y, attr, tile);
}

void rtv_blast(void)
{
    uint8_t i;
    for (i = 0; i < stage_count; i++) {
        uint8_t *vram = (uint8_t *)(0x9800
                        + (uint16_t)stage_map_y[i] * 32 + BOARD_OFFSET);
        if (LY_REG >= 144) {
            VBK_REG = VBK_ATTRIBUTES;
            set_data(vram, stage_attr[i], 16);
            VBK_REG = VBK_TILES;
            set_data(vram, stage_tile[i], 16);
            VBK_REG = VBK_ATTRIBUTES;
            set_data(vram + 32, &stage_attr[i][16], 16);
            VBK_REG = VBK_TILES;
            set_data(vram + 32, &stage_tile[i][16], 16);
        } else {
            VBK_REG = VBK_ATTRIBUTES;
            stat_safe_copy(vram, stage_attr[i], 16);
            VBK_REG = VBK_TILES;
            stat_safe_copy(vram, stage_tile[i], 16);
            VBK_REG = VBK_ATTRIBUTES;
            stat_safe_copy(vram + 32, &stage_attr[i][16], 16);
            VBK_REG = VBK_TILES;
            stat_safe_copy(vram + 32, &stage_tile[i][16], 16);
        }
    }
    VBK_REG = VBK_TILES;
    stage_count = 0;
}

void rtv_blit_board(const uint8_t board[8][8])
{
    uint8_t y;
    /* stage everything CPU-side first, then one blast */
    stage_count = 0;
    for (y = 0; y < 8; y++) {
        rtv_stage_board_row(board, y);
    }
    rtv_blast();
}
