/* gbforge runtime — staged board blits: DMA shadow + blast. See
 * vram.h. Banked (bank 3): HOME is nearly full and nothing here needs
 * to switch ROM banks — the DMA source is WRAM, the graphics tables
 * are in res/ headers that resolve to ROM-0 constants or copies. */

#pragma bank 3

#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>

#include "vram.h"

#include "tiles_data.h"
#include "palettes.h"

#define BOARD_OFFSET 1  /* board origin in bkg tile coords (x and y) */

/* ── staged blast: CGB DMA from a VRAM-shaped shadow ─────────────
 * Rows are staged CPU-side, then landed attr+tile so the LCD never
 * scans a mismatched pair.
 *
 * WHY DMA (measured with harness/scenarios/analyze_fall_flicker.c):
 * the previous STAT-polled C byte copy managed ~2.7 bytes per
 * scanline during active display — 24 scanlines per board row while
 * the beam crosses a board row in 16. Any blast deeper than the two
 * rows that fit in vblank had its middle rows TORN (beam inside the
 * row mid-write) and its bottom rows STALE (written after the beam
 * passed, shown a frame late), and a full-board blit spanned ~190
 * scanlines. General-purpose DMA moves 16 bytes in 32 dots: a whole
 * board (16 hw rows x 2 banks = 1 KB) lands in ~4.5 scanlines of
 * vblank with two register setups.
 *
 * SHADOW: DMA ignores the low 4 bits of both addresses and copies
 * contiguous bytes, so the source is a byte-exact shadow of BG map
 * rows 1..16 (all 32 columns, both banks). Columns 1..16 are the
 * board; column 0 / 17 carry the frame's left / right edge and
 * 18..31 the blank filler the modes keep there (puzzle's centred
 * scroll shows 18 and 31). Every board write goes through the shadow
 * (rtv_stage_row / rtv_blit_tile), so a dirty span can be blasted as
 * ONE DMA per bank and rows inside the span that were not restaged
 * are simply rewritten with what they already show.
 *
 * TIMING: rtv_blast() arms the VBlank handler in vram_vbl.c and waits
 * for it; both banks land at the start of vblank, ahead of the sound
 * driver, so a whole board (~4.5 scanlines) always fits. */

#define SHADOW_ROWS       16                  /* map rows 1..16 */
#define SHADOW_BANK_BYTES (SHADOW_ROWS * 32)

static uint8_t shadow_raw[2 * SHADOW_BANK_BYTES + 16];
static uint8_t *shadow;                       /* 16-aligned: [attr rows][tile rows] */
static uint8_t shadow_ready;
static uint8_t span_lo, span_hi;              /* dirty shadow rows; lo > hi = none */

static void ensure_shadow(void)
{
    uint8_t *pa, *pt;
    uint8_t y;
    if (shadow_ready) return;
    shadow_ready = 1;
    shadow = (uint8_t *)(((uint16_t)shadow_raw + 15) & 0xFFF0);
    pa = shadow;
    pt = shadow + SHADOW_BANK_BYTES;
    for (y = 0; y < SHADOW_ROWS; y++, pa += 32, pt += 32) {
        memset(pa, PAL_SILVER, 32);           /* frame + filler palette */
        memset(pt, UI_TILE_BLANK, 32);
        pt[0] = UI_TILE_BORDER_LEFT;          /* hud.c draw_border contract */
        pt[17] = UI_TILE_BORDER_RIGHT;
        memset(pt + 1, HW_TILE_BASE(0), 16);  /* empty cells until blitted */
    }
    span_lo = 0xFF;
    span_hi = 0;
}

void rtv_stage_reset(void) BANKED
{
    ensure_shadow();
    span_lo = 0xFF;
    span_hi = 0;
}

static void rtv_shadow_tile(uint8_t bx, uint8_t by,
                            const uint8_t attrs[4], const uint8_t tiles[4])
{
    uint8_t *pa;
    ensure_shadow();
    pa = shadow + (((uint16_t)by << 6) + 1 + (bx << 1));
    pa[0] = attrs[0];  pa[1] = attrs[1];
    pa[32] = attrs[2]; pa[33] = attrs[3];
    pa += SHADOW_BANK_BYTES;
    pa[0] = tiles[0];  pa[1] = tiles[1];
    pa[32] = tiles[2]; pa[33] = tiles[3];
}

void rtv_stage_row(uint8_t board_row,
                   const uint8_t attr[32], const uint8_t tile[32]) BANKED
{
    uint8_t hw = (uint8_t)(board_row << 1);
    uint8_t *pa = shadow + ((uint16_t)hw << 5);
    uint8_t *pt = pa + SHADOW_BANK_BYTES;
    memcpy(pa + 1, attr, 16);
    memcpy(pa + 33, attr + 16, 16);
    memcpy(pt + 1, tile, 16);
    memcpy(pt + 33, tile + 16, 16);
    if (hw < span_lo) span_lo = hw;
    if ((uint8_t)(hw + 1) > span_hi) span_hi = (uint8_t)(hw + 1);
}

void rtv_stage_board_row(const uint8_t board[8][8], uint8_t y) BANKED
{
    /* written straight into the shadow, pointer-walked: the temp
       attr[32]/tile[32] + memcpy version cost ~16 scanlines per row */
    uint8_t hw = (uint8_t)(y << 1);
    uint8_t *pa = shadow + ((uint16_t)hw << 5) + 1;
    uint8_t *pt = pa + SHADOW_BANK_BYTES;
    const uint8_t *row = board[y];
    uint8_t x;
    ensure_shadow();
    for (x = 0; x < 8; x++, pa += 2, pt += 2) {
        uint8_t t = row[x];
        uint8_t base = HW_TILE_BASE(t);
        const uint8_t *pm = tile_palette_map[t];
        pt[0] = base;      pt[1] = base + 1;
        pt[32] = base + 2; pt[33] = base + 3;
        pa[0] = pm[0];     pa[1] = pm[1];
        pa[32] = pm[2];    pa[33] = pm[3];
    }
    if (hw < span_lo) span_lo = hw;
    if ((uint8_t)(hw + 1) > span_hi) span_hi = (uint8_t)(hw + 1);
}

void rtv_blit_tile(uint8_t bx, uint8_t by, uint8_t type) BANKED
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

    /* keep the DMA shadow byte-exact with VRAM (see rtv_blast) */
    rtv_shadow_tile(bx, by, attrs, tiles);
}

/* Stage one logical tile into the shadow (no VRAM write): lands with
   the next rtv_blast, atomically with everything else staged. */
void rtv_stage_tile(uint8_t bx, uint8_t by, uint8_t type) BANKED
{
    uint8_t tiles[4];
    uint8_t attrs[4];
    uint8_t base = HW_TILE_BASE(type);
    uint8_t hw = (uint8_t)(by << 1);
    uint8_t i;
    for (i = 0; i < 4; i++) {
        tiles[i] = base + i;
        attrs[i] = tile_palette_map[type][i];
    }
    rtv_shadow_tile(bx, by, attrs, tiles);
    if (hw < span_lo) span_lo = hw;
    if ((uint8_t)(hw + 1) > span_hi) span_hi = (uint8_t)(hw + 1);
}

/* ── in-shadow motion primitives ──
 * A fall sub-step is literally "this cell's two hw rows move down one
 * hw row"; a refill sub-step is "the ghost stack moves down one hw row
 * and the next incoming hw row appears at the top". Doing that as byte
 * moves inside the shadow costs a few dozen cycles per cell, where
 * restaging whole rows from the board cost ~22 scanlines per row under
 * SDCC and dropped a vblank whenever four or more tiles fell. */

/* Fall sub-step for one board column: every cell in `mask` (bit y =
   cell y) moves its two hw rows down one hw row, walked bottom-up so a
   faller's bottom half is written after the cell below it has moved.
   phase 0 (half-step): rows (2y, 2y+1) -> (2y+1, 2y+2), the vacated
   row 2y becomes the empty tile's top half. phase 1 (full step): rows
   (2y+1, 2y+2) -> (2y+2, 2y+3), row 2y+1 becomes the empty bottom
   half. One banked call per column.
   The byte moves are hand-written asm: SDCC spent ~40 instructions of
   stack traffic on every `p[k] = p[j]`, ~2 scanlines per cell, and 35
   fallers overran the frame. */

static uint8_t *sh_ptr;      /* attr-bank pointer, row 0 of cell 6 (+phase) */
static uint8_t  sh_mask;     /* bit 6..0 = cell 6..0 */
static uint8_t  sh_t0;       /* fill tile (second byte = sh_t0 + 1) */
static uint8_t  sh_attr;     /* fill attr */

/* shift_rows3: hl -> row0 col; a = fill byte; b = second-byte delta
   (0 attr / 1 tile). (row+2) <- (row+1) <- (row0) <- fill for 2 columns. */
static void sh_asm(void) __naked
{
    __asm
        ld      hl, #_sh_ptr
        ld      a, (hl+)
        ld      h, (hl)
        ld      l, a                    ; hl = sh_ptr (attr bank, cell 6 row 0)
        ld      a, (_sh_mask)
        add     a, a                    ; bit 6 -> bit 7
        ld      c, a                    ; c = mask
        ld      b, #7                   ; cells 6..0
00001$:
        bit     7, c
        jr      z, 00002$
        push    bc
        push    hl
        ld      a, (_sh_attr)
        ld      c, a
        ld      b, #0
        call    00010$                  ; attr bank
        pop     hl
        push    hl
        ld      de, #512                ; SHADOW_BANK_BYTES
        add     hl, de
        ld      a, (_sh_t0)
        ld      c, a
        ld      b, #1
        call    00010$                  ; tile bank
        pop     hl
        pop     bc
00002$:
        ld      de, #0xFFC0             ; -64: one cell up
        add     hl, de
        sla     c
        dec     b
        jr      nz, 00001$
        ret
        ; ---- subroutine: hl = row0 col, c = fill, b = delta ----
00010$:
        ld      a, l                    ; hl -> row1
        add     a, #32
        ld      l, a
        jr      nc, 00011$
        inc     h
00011$:
        ld      a, (hl+)
        ld      e, a                    ; e = row1 col0
        ld      d, (hl)                 ; d = row1 col1 (hl -> row1 col1)
        ld      a, l                    ; hl -> row2 col1
        add     a, #32
        ld      l, a
        jr      nc, 00012$
        inc     h
00012$:
        ld      (hl), d
        dec     hl
        ld      (hl), e                 ; row2 <- row1
        ld      a, l                    ; hl -> row0 col0
        sub     a, #64
        ld      l, a
        jr      nc, 00013$
        dec     h
00013$:
        ld      a, (hl+)
        ld      e, a
        ld      d, (hl)                 ; de = row0 (hl -> row0 col1)
        ld      a, l                    ; hl -> row1 col1
        add     a, #32
        ld      l, a
        jr      nc, 00014$
        inc     h
00014$:
        ld      (hl), d
        dec     hl
        ld      (hl), e                 ; row1 <- row0
        ld      a, l                    ; hl -> row0 col0
        sub     a, #32
        ld      l, a
        jr      nc, 00015$
        dec     h
00015$:
        ld      (hl), c                 ; row0 <- fill
        inc     hl
        ld      a, c
        add     a, b
        ld      (hl), a
        ret
    __endasm;
}

void rtv_shift_column(uint8_t bx, uint8_t mask, uint8_t phase) BANKED
{
    uint8_t *pa = shadow + ((uint16_t)12 << 5) + 1 + (bx << 1);
    uint8_t lo, hi, bit;
    if (!mask) return;
    if (phase) pa += 32;
    sh_ptr = pa;
    sh_mask = mask;
    sh_t0 = (uint8_t)(HW_TILE_BASE(0) + (phase ? 2 : 0));  /* type 0 = empty */
    sh_attr = tile_palette_map[0][0];
    sh_asm();
    /* span: lowest / highest moving cell */
    lo = 0; bit = 1;
    while (!(mask & bit)) { bit <<= 1; lo++; }
    hi = 6; bit = 0x40;
    while (!(mask & bit)) { bit >>= 1; hi--; }
    lo = (uint8_t)((lo << 1) + phase);
    hi = (uint8_t)((hi << 1) + 2 + phase);
    if (lo < span_lo) span_lo = lo;
    if (hi > span_hi) span_hi = hi;
}

/* Refill conveyor: shift hw rows 0..k-1 of column bx down one row,
   then put the incoming hw row (tile pair / attr) at hw row 0. */
static uint8_t *cv_ptr;      /* attr-bank pointer, row k col */
static uint8_t  cv_k, cv_t0, cv_t1, cv_attr;

static void cv_asm(void) __naked
{
    __asm
        ld      hl, #_cv_ptr
        ld      a, (hl+)
        ld      h, (hl)
        ld      l, a                    ; hl = row k col (attr bank)
        ld      a, (_cv_attr)
        ld      d, a
        ld      e, a
        call    00020$
        ld      hl, #_cv_ptr
        ld      a, (hl+)
        ld      h, (hl)
        ld      l, a
        ld      de, #512
        add     hl, de
        ld      a, (_cv_t0)
        ld      d, a
        ld      a, (_cv_t1)
        ld      e, a
        call    00020$
        ret
        ; ---- hl = row k col; d,e = fill for col0,col1 ----
00020$:
        ld      a, (_cv_k)
        ld      b, a
        or      a
        jr      z, 00022$
00021$:
        push    de
        ld      a, l                    ; hl -> row-1 col0
        sub     a, #32
        ld      l, a
        jr      nc, 00023$
        dec     h
00023$:
        ld      a, (hl+)
        ld      e, a
        ld      d, (hl)                 ; de = row-1 (hl -> row-1 col1)
        ld      a, l                    ; hl -> row col1
        add     a, #32
        ld      l, a
        jr      nc, 00024$
        inc     h
00024$:
        ld      (hl), d
        dec     hl
        ld      (hl), e                 ; row <- row-1 (hl -> row col0)
        ld      a, l                    ; hl -> row-1 col0
        sub     a, #32
        ld      l, a
        jr      nc, 00025$
        dec     h
00025$:
        pop     de
        dec     b
        jr      nz, 00021$
00022$:
        ld      (hl), d                 ; row 0 <- incoming
        inc     hl
        ld      (hl), e
        ret
    __endasm;
}

void rtv_conveyor_step(uint8_t bx, uint8_t k,
                       uint8_t t0, uint8_t t1, uint8_t attr) BANKED
{
    cv_ptr = shadow + (((uint16_t)k << 5) + 1 + (bx << 1));
    cv_k = k; cv_t0 = t0; cv_t1 = t1; cv_attr = attr;
    cv_asm();
    span_lo = 0;
    if (k > span_hi) span_hi = k;
}

/* Overwrite one cell's four attr bytes in the shadow (tiles untouched)
   — the match flash paints matched cells silver on top of a row
   staged by rtv_stage_board_row. */
void rtv_stage_cell_attr(uint8_t bx, uint8_t by, uint8_t attr) BANKED
{
    uint8_t *pa = shadow + (((uint16_t)by << 6) + 1 + (bx << 1));
    uint8_t hw = (uint8_t)(by << 1);
    pa[0] = attr;  pa[1] = attr;
    pa[32] = attr; pa[33] = attr;
    if (hw < span_lo) span_lo = hw;
    if ((uint8_t)(hw + 1) > span_hi) span_hi = (uint8_t)(hw + 1);
}

/* general-purpose DMA: blocks x 16 bytes; the CPU stalls until done */
static void gdma(const uint8_t *src, uint16_t dst, uint8_t blocks)
{
    HDMA1_REG = (uint8_t)((uint16_t)src >> 8);
    HDMA2_REG = (uint8_t)src;
    HDMA3_REG = (uint8_t)(dst >> 8);
    HDMA4_REG = (uint8_t)dst;
    HDMA5_REG = (uint8_t)(blocks - 1);
}

extern volatile uint8_t rtv_vbl_armed;
extern uint8_t rtv_vbl_installed;
extern const uint8_t *rtv_vbl_src_attr, *rtv_vbl_src_tile;
extern uint16_t rtv_vbl_dst;
extern uint8_t rtv_vbl_blocks;
void rtv_blast_now(void);

void rtv_blast(void) BANKED
{
    uint8_t blocks;
    const uint8_t *sa;

    if (span_lo > span_hi) { vsync(); return; }
    blocks = (uint8_t)((span_hi - span_lo + 1) << 1);
    sa = shadow + ((uint16_t)span_lo << 5);
    rtv_vbl_src_attr = sa;
    rtv_vbl_src_tile = sa + SHADOW_BANK_BYTES;
    rtv_vbl_dst = (uint16_t)(0x9800 + (((uint16_t)span_lo + BOARD_OFFSET) << 5));
    rtv_vbl_blocks = blocks;
    span_lo = 0xFF;
    span_hi = 0;
    if (!(LCDC_REG & 0x80)) {           /* display off: no vblank ISR */
        rtv_vbl_armed = 1;
        rtv_blast_now();
        return;
    }
    rtv_vbl_armed = 1;
    if (!rtv_vbl_installed) {
        /* An entry point that never called rtv_blast_install(): land it
           ourselves right after vsync() instead — a few lines later
           than the ISR path, still inside vblank for anything but a
           whole board after a long ISR, and never a wild jump (a
           smoketest main without the install rebooted on its first
           swap). */
        vsync();
        rtv_blast_now();
        return;
    }
    /* land it in the next vblank's ISR (vram_vbl.c), both banks in one
       go at LY ~144; vsync() returns once the handler chain has run */
    vsync();
}

void rtv_blit_board(const uint8_t board[8][8]) BANKED
{
    uint8_t y;
    /* Stage everything CPU-side FIRST — the generic row staging runs
       ~16 scanlines per row under SDCC — and only then sync to
       vblank for the blast. The legacy "vsync(); rtv_blit_board()"
       order put the sync before the staging, so the copy itself
       started around LY 124 and painted through the whole next
       frame. Callers need not vsync (harmless if they do). */
    rtv_stage_reset();
    for (y = 0; y < 8; y++) {
        rtv_stage_board_row(board, y);
    }
    rtv_blast();                        /* waits for the vblank itself */
}
