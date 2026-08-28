/* gbforge runtime — resolution-script playback. See anim.h. */

/* Banked placement under GBDK: these functions are __banked
   (see engine.h) so they live outside HOME, which the ng ROM
   nearly filled. Host builds ignore this. */
#ifdef __SDCC
#pragma bank 1
#endif

#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>

#include "engine.h"
#include "anim.h"
#include "sound_glue.h"
#include "cursor.h"
#include "hud.h"
#ifdef DEBUG_BUILD
#include "debug.h"
#endif
#include "vram.h"
#include "hud.h"

#include "palettes.h"     /* PAL_SILVER — editor-owned contract */
#include "tiles_data.h"

/* Parameter data comes from gen/anim_tables.c (anim_params_rom),
 * emitted by gbforge gen_anim from the AnimationSpec definitions. */

#ifdef DEBUG_BUILD
rt_anim_params anim_params;
#endif

static uint8_t visual[8][8];

#define BOARD_OFFSET 1

/* Swap slide resources (sprites_data.h editor contract): borrowed
 * sprite tiles/OAM/palettes, same slots the legacy swap used. */
#include "sprites_data.h"

/* ── screen shake (legacy render.c state machine) ── */

static uint8_t shake_remaining;
static uint8_t shake_base_scx;
static uint8_t shake_base_scy;

void rta_start_shake(void) BANKED
{
    /* Re-arming mid-shake must NOT recapture the base — SCX/SCY are
       currently displaced and the screen would drift permanently
       (latent hazard in the legacy version too). */
    if (shake_remaining == 0) {
        shake_base_scx = SCX_REG;
        shake_base_scy = SCY_REG;
    }
    shake_remaining = RTA_PARAMS->shake_frames;
}

void rta_apply_shake(void) BANKED
{
    if (shake_remaining > 0) {
        uint8_t idx = RTA_PARAMS->shake_frames - shake_remaining;
        SCX_REG = (uint8_t)(shake_base_scx + RTA_PARAMS->shake_dx[idx]);
        SCY_REG = (uint8_t)(shake_base_scy + RTA_PARAMS->shake_dy[idx]);
        shake_remaining--;
    }
}

/* ── floating "+N" numbers (legacy render.c float system) ── */

#define FLOAT_FRAMES 24
static const uint8_t float_y_offset[FLOAT_FRAMES] = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
};
static const uint8_t float_palette[FLOAT_FRAMES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 2, 2, 2, 3, 3, 3, 3
};

static struct {
    uint8_t active;
    uint8_t sx, sy;
    uint8_t frame;
} floats[MAX_FLOATING_NUMS];
static uint8_t float_count;

/* puzzle mode suppresses floats (no resource counters there) and
   reuses their OAM slots for the "MOVE: XX" counter */
uint8_t rta_floats_enabled = 1;

static void spawn_float(uint8_t gx, uint8_t gy, uint8_t value)
{
    uint8_t i, spr;
    if (!rta_floats_enabled) return;
    for (i = 0; i < MAX_FLOATING_NUMS; i++) {
        if (floats[i].active) continue;
        floats[i].active = 1;
        floats[i].sx = (uint8_t)((gx << 4) + rt_screen_off_x + 4 + 8);
        floats[i].sy = (uint8_t)((gy << 4) + RT_SCREEN_OFF_Y + 4 + 16);
        floats[i].frame = 0;
        float_count++;
        spr = FLOAT_SPRITE_BASE + (i << 1);
        set_sprite_tile(spr, FLOAT_TILE_PLUS);
        set_sprite_tile(spr + 1, (value >= 10)
                        ? KNOWLEDGE_TILE_0 + 1
                        : KNOWLEDGE_TILE_0 + value);
        return;
    }
}

void rta_update_floats(void) BANKED
{
    uint8_t i, spr, y;
    if (float_count == 0) return;
    spr = FLOAT_SPRITE_BASE;
    for (i = 0; i < MAX_FLOATING_NUMS; i++, spr += 2) {
        if (!floats[i].active) continue;
        floats[i].frame++;
        if (floats[i].frame >= FLOAT_FRAMES) {
            floats[i].active = 0;
            float_count--;
            move_sprite(spr, 0, 0);
            move_sprite(spr + 1, 0, 0);
            continue;
        }
        y = floats[i].sy - float_y_offset[floats[i].frame];
        set_sprite_prop(spr, float_palette[floats[i].frame]);
        set_sprite_prop(spr + 1, float_palette[floats[i].frame]);
        move_sprite(spr, floats[i].sx, y);
        move_sprite(spr + 1, floats[i].sx + 8, y);
    }
}

/* Per-frame hook: the game's input/cursor pump (the legacy
   render_delay_with_cursor role). Runs once per animation frame at
   a VRAM-safe point — after any staged blast, never between vsync
   and the blanking-window writes. NULL = no pump. */
static void (*frame_hook)(void);

/* parameter accessors for out-of-bank consumers (the tutorial
   animates with the SAME curves/holds as gameplay) */
uint8_t rta_p_swap_frames(void) BANKED { return RTA_PARAMS->swap_frames; }
uint8_t rta_p_swap_off(uint8_t f) BANKED { return RTA_PARAMS->swap_curve[f]; }
uint8_t rta_p_flash_hold(void) BANKED { return RTA_PARAMS->flash_hold; }
uint8_t rta_p_sparkle_frames(void) BANKED { return RTA_PARAMS->sparkle_frames; }

void rta_set_frame_hook(void (*fn)(void)) BANKED
{
    frame_hook = fn;
}

#define FRAME_HOOK() do { if (frame_hook) frame_hook(); } while (0)

/* One animation frame's worth of the non-blocking systems (shake,
   "+N" floats). Every site that does its own vsync()+blast must call
   this too — the fall and refill loops used to skip it, so a manna
   float spawned over the falling column froze on every half/full-step
   frame and lived ~47 frames instead of 24 (bead GameBoyGames-3at). */
static void frame_tick(void)
{
    rta_apply_shake();
    rta_update_floats();
}

static void wait_frames(uint8_t n)
{
    /* every blocking wait pumps the non-blocking systems, like the
       legacy render_delay_with_cursor frame pump */
    while (n--) {
        vsync();
        frame_tick();
        FRAME_HOOK();
    }
}

void rta_frame_pump(void) BANKED
{
    vsync();
    frame_tick();
    FRAME_HOOK();
}

void rta_init(const uint8_t board[8][8]) BANKED
{
#ifdef DEBUG_BUILD
    memcpy(&anim_params, &anim_params_rom, sizeof(anim_params));
#endif
    rta_sync(board);
}

void rta_sync(const uint8_t board[8][8]) BANKED
{
    memcpy(visual, board, 64);
}

/* Flash matched cells: attributes only (sprite-free, per the legacy
 * interlace fix) — all four quadrants to PAL_SILVER. */
/* set by rta_flash_early: the flash is already on screen and landed
   at sys_time == flash_t0 (GBDK's VBL frame counter) */
static uint8_t flash_early;
static uint16_t flash_t0;

static void flash_matched(const uint8_t row_masks[8])
{
    uint8_t x, y;

    if (flash_early) {
        /* already showing: pump until flash_hold frames have passed
           since it landed (the engine work ran under it) */
        flash_early = 0;
        while ((uint16_t)(sys_time - flash_t0) < RTA_PARAMS->flash_hold)
            wait_frames(1);
        return;
    }

    /* staged + vblank-blasted like every other board update (direct
       attr writes landed wherever the beam was). Rows are staged by
       the fast row primitive and only the matched cells get their
       attrs painted silver: the per-cell C loop here cost ~27
       scanlines per row (bead GameBoyGames-374). */
    rtv_stage_reset();
    for (y = 0; y < 8; y++) {
        uint8_t mask = row_masks[y];
        uint8_t xbit = 1;
        if (!mask) continue;
        rtv_stage_board_row(visual, y);
        for (x = 0; x < 8; x++, xbit <<= 1)
            if (mask & xbit) rtv_stage_cell_attr(x, y, PAL_SILVER);
    }
    rtv_blast();
    frame_tick();
    wait_frames(RTA_PARAMS->flash_hold);
}

void rta_flash_early(const uint8_t matched[8][8]) BANKED
{
    uint8_t masks[8];
    uint8_t x, y;
    for (y = 0; y < 8; y++) {
        const uint8_t *m = matched[y];
        uint8_t mask = 0, xbit = 1;
        for (x = 0; x < 8; x++, xbit <<= 1)
            if (m[x]) mask |= xbit;
        masks[y] = mask;
    }
    flash_early = 0;
    rtv_stage_reset();
    for (y = 0; y < 8; y++) {
        uint8_t mask = masks[y];
        uint8_t xbit = 1;
        if (!mask) continue;
        rtv_stage_board_row(visual, y);
        for (x = 0; x < 8; x++, xbit <<= 1)
            if (mask & xbit) rtv_stage_cell_attr(x, y, PAL_SILVER);
    }
    rtv_blast();
    frame_tick();
    FRAME_HOOK();
    flash_t0 = sys_time;
    flash_early = 1;
}

/* rta_play's own frame is large (event buffers); keep the fall list
   out of it so SDCC can address the hot loops directly. */
static struct { uint8_t x, cur, to, tile; } falls[64];
static uint8_t rf_empty[8];    /* refill: ghosts per column */
static const uint8_t bit_of8[8] = {1, 2, 4, 8, 16, 32, 64, 128};

void rta_play(const rt_engine *e) BANKED
{
    uint8_t row_masks[8];
    uint8_t n_falls = 0;
    uint8_t i, x, y, step;

    memset(row_masks, 0, sizeof(row_masks));

    /* First sweep: collect masks (pointer-walked: indexing
       e->events[i] cost ~40 scanlines per pass over the buffer) */
    {
        const rt_event *ev = e->events;
        const rt_event *end = e->events + e->event_count;
        for (; ev < end; ev++)
            if (ev->type == RT_EV_MATCH_ROW) row_masks[ev->a] = ev->b;
    }
    /* 1) flash the matched cells, hold */
    flash_matched(row_masks);

    /* 2) clear matched from the visual board, apply transmutes,
       spawn +N floats at the flood centroids, blit */
    for (y = 0; y < 8; y++) {
        uint8_t mask = row_masks[y];
        uint8_t xbit = 1;
        if (!mask) continue;
        for (x = 0; x < 8; x++, xbit <<= 1)
            if (mask & xbit) visual[y][x] = RT_EMPTY;
    }
    {
        uint8_t spark_x[3], spark_y[3];
        uint8_t n_sparks = 0;
        uint8_t f;
        uint8_t dirty = 0, ybit = 1;
        uint8_t sfx_tiers = 0;          /* transmute jingles played (bit = tier-4) */
        const rt_event *ev = e->events;
        const rt_event *end = e->events + e->event_count;

        for (y = 0; y < 8; y++, ybit <<= 1)
            if (row_masks[y]) dirty |= ybit;
        for (; ev < end; ev++) {
            if (ev->type == RT_EV_TRANSMUTE || ev->type == RT_EV_BONUS) {
                visual[ev->b][ev->a] = ev->c;
                dirty |= bit_of8[ev->b];
                if (ev->c >= 4 && ev->c <= 11 &&
                    !(sfx_tiers & bit_of8[ev->c - 4])) {
                    /* one jingle per tier per pass: a jingle is ~19
                       scanlines of synchronous APU writes and five
                       transmutes in a pass overran the frame (the
                       repeats were inaudible anyway — the last one
                       restarted the same channels). Pump between the
                       ones that remain. */
                    if (sfx_tiers) rta_frame_pump();
                    sfx_tiers |= bit_of8[ev->c - 4];
                    ngau_event(NGAU_EV_TRANSMUTE_BRONZE + (ev->c - 4));
                }
                if (n_sparks < 3) {
                    spark_x[n_sparks] = ev->a;
                    spark_y[n_sparks] = ev->b;
                    n_sparks++;
                }
            } else if (ev->type == RT_EV_AWARD_MANNA) {
                spawn_float(ev->c >> 4, ev->c & 0x0F, ev->b);
            } else if (ev->type == RT_EV_AWARD_KNOW) {
                /* knowledge feedback: +N at the match (digit caps
                   at 9; the K counter carries the exact total) */
                uint8_t kp = ev->a;
                if (ev->b || kp > 9) kp = 9;
                spawn_float(ev->c >> 4, ev->c & 0x0F, kp);
            }
        }
        /* the cleared/transmuted rows + the sparkle cells hidden, in
           ONE vblank blast. Only rows that changed are restaged
           (matched rows, plus the rows of transmute / bonus cells): a
           full 8-row restage cost ~100 scanlines and a vblank. */
        /* the sweep above (transmute SFX at ~19 scanlines each, float
           spawns) plus the staging overran the frame on multi-
           transmute passes: pump once before staging */
        rta_frame_pump();
        rtv_stage_reset();
        ybit = 1;
        for (y = 0; y < 8; y++, ybit <<= 1)
            if (dirty & ybit) rtv_stage_board_row(visual, y);
        for (i = 0; i < n_sparks; i++) {
            rtv_stage_tile(spark_x[i], spark_y[i], RT_EMPTY);
            set_sprite_prop(BURST_SPRITE_BASE + i, 0);
        }
        rtv_blast();
        frame_tick();
        FRAME_HOOK();

        /* 2b) transmute sparkle: hide the new tiles behind a 3-phase
           sprite burst (dot → small → full), then reveal */
        if (n_sparks) {
            for (f = 0; f < 3; f++) {
                for (i = 0; i < n_sparks; i++) {
                    set_sprite_tile(BURST_SPRITE_BASE + i,
                                    BURST_TILE_DOT + f);
                    move_sprite(BURST_SPRITE_BASE + i,
                                (spark_x[i] << 4) + rt_screen_off_x + 4 + 8,
                                (spark_y[i] << 4) + RT_SCREEN_OFF_Y + 4 + 16);
                }
                wait_frames(RTA_PARAMS->sparkle_frames);
            }
            rtv_stage_reset();
            for (i = 0; i < n_sparks; i++) {
                move_sprite(BURST_SPRITE_BASE + i, 0, 0);
                rtv_stage_tile(spark_x[i], spark_y[i],
                               visual[spark_y[i]][spark_x[i]]);
            }
            rtv_blast();
            frame_tick();
        }
    }
    wait_frames(RTA_PARAMS->clear_hold);

    /* 3) falls: step every falling tile down one cell per step,
       delay per the acceleration curve */
    /* FALL events sit between the process events and the trailing
       REFILL events (flow: process -> gravity -> refill), so walk the
       buffer backwards and stop at the first non-fall: O(falls), not
       O(events) — the forward scan alone cost ~40 scanlines. */
    {
        const rt_event *end = e->events + e->event_count;
        const rt_event *ev;
        while (end > e->events && (end - 1)->type == RT_EV_REFILL) end--;
        ev = end;
        while (ev > e->events && (ev - 1)->type == RT_EV_FALL) ev--;
        /* collect FORWARD: emission order is per-column bottom-up, and
           the move loop below relies on it (a faller stepping into the
           cell below must not overwrite one that has not moved yet) */
        for (; ev < end && n_falls < 64; ev++) {
            falls[n_falls].x = ev->a;
            falls[n_falls].cur = ev->b;
            falls[n_falls].to = ev->c;
            falls[n_falls].tile = visual[ev->b][ev->a];
            n_falls++;
        }
    }
    if (n_falls) ngau_event(NGAU_EV_FALL);
    /* Half-step technique (legacy render_gravity_drop): each 16 px
       fall step renders as two 8 px sub-frames. The half frame shows
       every falling tile straddling two cells — its bottom hw-row in
       the cell below, its top hw-row in its own cell's bottom — then
       the full frame lands it. Staged + blasted, tear-free. */
    step = 0;
    while (1) {
        uint8_t col_mask[8];
        uint8_t any = 0;
        memset(col_mask, 0, sizeof(col_mask));
        for (i = 0; i < n_falls; i++) {
            if (falls[i].cur < falls[i].to) {
                col_mask[falls[i].x] |= bit_of8[falls[i].cur];
                any = 1;
            }
        }
        if (!any) break;
        /* (no FRAME_HOOK here: the pump already ran after the previous
           blast / wait frame — a second cursor draw in the same frame
           cost up to ~40 scanlines and, with 30+ fallers, the vblank) */
        /* HALF-STEP: every moving cell's hw rows slide down one row in
           the shadow (rtv_shift_column, one call per column); the
           vacated top row becomes the empty tile's top half. Each hw
           row keeps its own tile's palette — the legacy "unify palettes
           vertically" copy painted every faller's top half in the
           palette of the tile above it (the half-piece flicker). */
        rtv_stage_reset();
        for (x = 0; x < 8; x++)
            if (col_mask[x]) rtv_shift_column(x, col_mask[x], 0);
        rtv_blast();
        frame_tick();
        FRAME_HOOK();

        /* FULL-STEP: slide once more — the tile lands in the cell
           below, its old cell's bottom row becomes empty */
        rtv_stage_reset();
        for (x = 0; x < 8; x++)
            if (col_mask[x]) rtv_shift_column(x, col_mask[x], 1);
        /* MOVE one cell in the logical board */
        for (i = 0; i < n_falls; i++) {
            if (falls[i].cur < falls[i].to) {
                visual[falls[i].cur][falls[i].x] = RT_EMPTY;
                falls[i].cur++;
                visual[falls[i].cur][falls[i].x] = falls[i].tile;
            }
        }
        rtv_blast();
        frame_tick();
        FRAME_HOOK();
        {
            uint8_t d = RTA_PARAMS->gravity_delay[step > 7 ? 7 : step];
            if (d > 1) wait_frames(d - 1);
        }
        step++;
    }

    /* 4) refills (endless modes): the new tiles drop in from above
       as ghost (faded) tiles, one row per step, exactly like legacy
       render_refill_drop — never a pop-in. After landing, a short
       ghost hold, then the real tiles reveal. */
    {
        uint8_t max_empty = 0;

        memset(rf_empty, 0, sizeof(rf_empty));
        {
            const rt_event *ev = e->events + e->event_count;
            while (ev > e->events && (ev - 1)->type == RT_EV_REFILL) {
                ev--;
                visual[ev->b][ev->a] = ev->c;
                rf_empty[ev->a]++;
            }
        }
        for (x = 0; x < 8; x++)
            if (rf_empty[x] > max_empty) max_empty = rf_empty[x];

        if (max_empty) {
            ngau_event(NGAU_EV_REFILL);
            for (step = 0; step < max_empty; step++) {
                uint8_t sub;
                /* two conveyor sub-steps per row (half, full): the
                   ghost stack enters bottom tile first, bottom half
                   first. A column whose stack has fully arrived shows
                   its real tiles from its next sub-step on (legacy). */
                for (sub = 0; sub < 2; sub++) {
                    uint8_t k = (uint8_t)((step << 1) + sub);
                    rtv_stage_reset();
                    for (x = 0; x < 8; x++) {
                        uint8_t n = rf_empty[x];
                        if (k < (uint8_t)(n << 1)) {
                            uint8_t t = visual[n - 1 - (k >> 1)][x];
                            /* REAL tile halves, not the faded
                               ghosts: manna corner subtiles are
                               mostly background, so the ghost
                               transform left them near-blank — the
                               reported one-frame white half-tile
                               during refill */
                            uint8_t base = HW_TILE_BASE(t);
                            uint8_t pal = tile_palette_map[t][0];
                            if (sub == 0)      /* bottom half enters */
                                rtv_conveyor_step(x, k, base + 2, base + 3, pal);
                            else               /* then its top half */
                                rtv_conveyor_step(x, k, base, base + 1, pal);
                        } else if (n && k == (uint8_t)(n << 1)) {
                            /* stack landed: real tiles (legacy reveal) */
                            for (y = 0; y < n; y++)
                                rtv_stage_tile(x, y, visual[y][x]);
                        }
                    }
                    rtv_blast();
                    frame_tick();
                    FRAME_HOOK();
                }
                {
                    uint8_t d = RTA_PARAMS->gravity_delay[
                        step > 7 ? 7 : step];
                    if (d > 1) wait_frames(d - 1);
                }
            }
            /* ghost hold, then reveal the real tiles */
            wait_frames(8);
        }
    }
    rtv_blit_board(visual);          /* stages, then syncs to vblank */
    frame_tick();
    FRAME_HOOK();
    wait_frames(RTA_PARAMS->pass_gap);
}

/* ── warning flash (legacy no-moves punish cue: 3 × 10 frames) ── */

void rta_play_warning(const uint8_t board[8][8]) BANKED
{
    uint8_t grey[2] = {PAL_SILVER, PAL_SILVER};
    uint8_t cycle, x, y;

    for (cycle = 0; cycle < 3; cycle++) {
        VBK_REG = VBK_ATTRIBUTES;
        for (y = 0; y < 16; y++) {
            for (x = 0; x < 8; x++) {
                set_bkg_tiles(BOARD_OFFSET + (x << 1),
                              BOARD_OFFSET + y, 2, 1, grey);
            }
        }
        VBK_REG = VBK_TILES;
        wait_frames(10);
        rtv_blit_board(board);  /* restores attrs + tiles */
        frame_tick();
        wait_frames(10);
    }
}

/* ── hint wiggle (legacy render_hint_wiggle mechanics) ── */

/* the ghost tile rides the SWAP sprite slots (25-28): they are
   idle while a hint plays, whereas the hint slots 36-39 carry the
   MOVES counter in endless — the wiggle used to make it vanish */
void rta_play_wiggle(uint8_t gx, uint8_t gy) BANKED
{
    uint8_t tile_type = visual[gy][gx];
    uint8_t sx = (uint8_t)((gx << 4) + rt_screen_off_x + 8);
    uint8_t sy = (uint8_t)((gy << 4) + RT_SCREEN_OFF_Y + 16);
    uint8_t frame, i;
    uint8_t total = RTA_PARAMS->wiggle_steps *
                    RTA_PARAMS->wiggle_step_frames;

    if (tile_type == RT_EMPTY) return;

    rtv_load_hint_sprite(tile_type);
    /* OBJ palettes 4-6 borrow the tile's BG colors; quadrant 3
       reuses slot 4 so slot 7 (cursor white) stays untouched —
       the legacy bug-4 fix, preserved. */
    for (i = 0; i < 4; i++) {
        uint8_t pal_slot = (i == 3) ? 4 : (uint8_t)(4 + i);
        if (i != 3) {
            set_sprite_palette(pal_slot, 1,
                &bg_palettes[tile_palette_map[tile_type][i] * 4]);
        }
        set_sprite_tile(SWAP_SPRITE_A_BASE + i, HINT_TILE_BASE + i);
        set_sprite_prop(SWAP_SPRITE_A_BASE + i, pal_slot);
    }

    rtv_blit_tile(gx, gy, RT_EMPTY);

    for (frame = 0; frame < total; frame++) {
        uint8_t idx = frame / RTA_PARAMS->wiggle_step_frames;
        int8_t o;
        if (idx >= RTA_PARAMS->wiggle_steps)
            idx = RTA_PARAMS->wiggle_steps - 1;
        o = RTA_PARAMS->wiggle_offsets[idx];
        move_sprite(SWAP_SPRITE_A_BASE + 0, sx + o, sy + o);
        move_sprite(SWAP_SPRITE_A_BASE + 1, sx + 8 + o, sy + o);
        move_sprite(SWAP_SPRITE_A_BASE + 2, sx + o, sy + 8 + o);
        move_sprite(SWAP_SPRITE_A_BASE + 3, sx + 8 + o, sy + 8 + o);
        wait_frames(1);
    }

    for (i = 0; i < 4; i++) {
        move_sprite(SWAP_SPRITE_A_BASE + i, 0, 0);
    }
    rtv_blit_tile(gx, gy, tile_type);

    /* One frame so the OAM DMA actually hides the ghost sprites —
       restoring the palettes while they are still on screen tints
       the tile red/gray for a frame (the reported flicker). */
    wait_frames(1);

    /* the wiggle borrowed OBJ palettes 4-6 for the ghost tile:
       restore them (opponent red + ghost grays) and force the
       cursor to re-assert its own palette on the next draw —
       otherwise the CPU cursor, the opponent counter, and the
       hint brackets keep whatever gem colors the wiggle loaded */
    rtc_hint_palettes();
    rtc_invalidate();
}

/* ── sprite tile slide (legacy render_swap_anim mechanics) ── */

static void swap_sprites_off(void)
{
    uint8_t i;
    for (i = 0; i < 4; i++) {
        move_sprite(SWAP_SPRITE_A_BASE + i, 0, 0);
        move_sprite(SWAP_SPRITE_B_BASE + i, 0, 0);
    }
}

static void place_quads(uint8_t base_slot, uint8_t px, uint8_t py)
{
    /* 2x2 quadrant layout: +8/+16 GBC sprite offsets included */
    move_sprite(base_slot + 0, px + 8, py + 16);
    move_sprite(base_slot + 1, px + 16, py + 16);
    move_sprite(base_slot + 2, px + 8, py + 24);
    move_sprite(base_slot + 3, px + 16, py + 24);
}

void rta_play_swap(uint8_t x1, uint8_t y1,
                   uint8_t x2, uint8_t y2) BANKED
{
    uint8_t type1 = visual[y1][x1];
    uint8_t type2 = visual[y2][x2];
    uint8_t ax = rt_screen_off_x + (x1 << 4);
    uint8_t ay = RT_SCREEN_OFF_Y + (y1 << 4);
    uint8_t bx = rt_screen_off_x + (x2 << 4);
    uint8_t by = RT_SCREEN_OFF_Y + (y2 << 4);
    int8_t dx = (x2 > x1) ? 1 : (x2 < x1) ? -1 : 0;
    int8_t dy = (y2 > y1) ? 1 : (y2 < y1) ? -1 : 0;
    uint8_t f, i, off;
    uint8_t n = RTA_PARAMS->swap_frames;

    rtv_load_swap_sprites(type1, type2);

    for (i = 0; i < 4; i++) {
        set_sprite_tile(SWAP_SPRITE_A_BASE + i, SWAP_TILE_A + i);
        set_sprite_prop(SWAP_SPRITE_A_BASE + i, SWAP_PAL_A);
        set_sprite_tile(SWAP_SPRITE_B_BASE + i, SWAP_TILE_B + i);
        set_sprite_prop(SWAP_SPRITE_B_BASE + i, SWAP_PAL_B);
    }

    /* lift both tiles off the BG */
    visual[y1][x1] = RT_EMPTY;
    visual[y2][x2] = RT_EMPTY;
    vsync();
    rtv_blit_tile(x1, y1, RT_EMPTY);
    rtv_blit_tile(x2, y2, RT_EMPTY);

    for (f = 0; f < n; f++) {
        off = RTA_PARAMS->swap_curve[f];
        place_quads(SWAP_SPRITE_A_BASE,
                    ax + dx * off, ay + dy * off);
        place_quads(SWAP_SPRITE_B_BASE,
                    bx - dx * off, by - dy * off);
        /* the grabber bracket rides tile A: pin it to the SAME
           easing curve as the sprites — the frame pump's fixed-
           speed glide ran at a different rate (reported desync) */
        rtc_ride((int16_t)((x1 << 4) + dx * off),
                 (int16_t)((y1 << 4) + dy * off));
        vsync();
        frame_tick();
        FRAME_HOOK();
    }

    /* land exchanged */
    visual[y1][x1] = type2;
    visual[y2][x2] = type1;
    vsync();
    rtv_blit_tile(x1, y1, visual[y1][x1]);
    rtv_blit_tile(x2, y2, visual[y2][x2]);
    swap_sprites_off();

    /* The slide borrowed OBJ palettes 4/5 for the two tiles'
       colors — slot 4 is the battle red, slot 5 the ghost hint
       gray, so a water swap left the next hint bracket blue. One
       frame for the OAM hide to land, then restore. The bracket
       stays pinned on the landed tile through that frame — the hook
       would otherwise glide it toward the (possibly not yet
       corrected) logical cell. */
    rtc_ride((int16_t)(x2 << 4), (int16_t)(y2 << 4));
    wait_frames(1);
    rtc_hint_palettes();
    rtc_invalidate();
}
