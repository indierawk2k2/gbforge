/* gbforge runtime — standard HUD. See hud.h. Ported from legacy
 * render_border / render_ui_layout / render_ui /
 * render_knowledge_sprites; all tile ids and palettes come from the
 * editor-owned res contracts. */

#ifdef __SDCC
#pragma bank 2
#endif

#include <gb/gb.h>
#include <gb/cgb.h>

#include "hud.h"

#include "tiles_data.h"
#include "palettes.h"
#include "sprites_data.h"
#include "ability.h"

#define KNOWLEDGE_PAL 0
#define KNOWLEDGE_SCREEN_X 3
#define KNOWLEDGE_SCREEN_Y 135

/* digit_tens[n] = n/10, digit_ones[n] = n%10 — no division in the
 * update path (legacy tables). */
static const uint8_t digit_tens[100] = {
    0,0,0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,4,4,4, 5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6, 7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8, 9,9,9,9,9,9,9,9,9,9
};
static const uint8_t digit_ones[100] = {
    0,1,2,3,4,5,6,7,8,9, 0,1,2,3,4,5,6,7,8,9,
    0,1,2,3,4,5,6,7,8,9, 0,1,2,3,4,5,6,7,8,9,
    0,1,2,3,4,5,6,7,8,9, 0,1,2,3,4,5,6,7,8,9,
    0,1,2,3,4,5,6,7,8,9, 0,1,2,3,4,5,6,7,8,9,
    0,1,2,3,4,5,6,7,8,9, 0,1,2,3,4,5,6,7,8,9
};

static uint16_t last_knowledge;
static uint8_t knowledge_visible = 1;

static void draw_border(void)
{
    uint8_t attr_row[18];
    uint8_t tile_row[18];
    uint8_t i;

    for (i = 0; i < 18; i++) attr_row[i] = PAL_SILVER;
    VBK_REG = VBK_ATTRIBUTES;
    set_bkg_tiles(0, 0, 18, 1, attr_row);
    set_bkg_tiles(0, 17, 18, 1, attr_row);
    {
        uint8_t attr = PAL_SILVER;
        for (i = 1; i < 17; i++) {
            set_bkg_tiles(0, i, 1, 1, &attr);
            set_bkg_tiles(17, i, 1, 1, &attr);
        }
    }

    VBK_REG = VBK_TILES;
    tile_row[0] = UI_TILE_CORNER_TL;
    for (i = 1; i < 17; i++) tile_row[i] = UI_TILE_BORDER_TOP;
    tile_row[17] = UI_TILE_CORNER_TR;
    set_bkg_tiles(0, 0, 18, 1, tile_row);

    tile_row[0] = UI_TILE_CORNER_BL;
    for (i = 1; i < 17; i++) tile_row[i] = UI_TILE_BORDER_BOT;
    tile_row[17] = UI_TILE_CORNER_BR;
    set_bkg_tiles(0, 17, 18, 1, tile_row);
    {
        uint8_t left = UI_TILE_BORDER_LEFT;
        uint8_t right = UI_TILE_BORDER_RIGHT;
        for (i = 1; i < 17; i++) {
            set_bkg_tiles(0, i, 1, 1, &left);
            set_bkg_tiles(17, i, 1, 1, &right);
        }
    }
}

static void draw_sidebar_layout(void)
{
    uint8_t row;

    VBK_REG = VBK_ATTRIBUTES;
    for (row = 0; row < 18; row++) {
        uint8_t attrs[3] = { PAL_SILVER, PAL_SILVER, PAL_SILVER };
        set_win_tiles(0, row, 3, 1, attrs);
    }
    {
        /* one counter row per tile kind, dot in the kind's own
           palette (palette i colors kind i in the asset pack) */
        uint8_t k;
        for (k = 0; k < 4; k++) {
            uint8_t attrs[3] = { 0, PAL_SILVER, PAL_SILVER };
            attrs[0] = k;
            set_win_tiles(0, k, 3, 1, attrs);
        }
    }

    VBK_REG = VBK_TILES;
    for (row = 0; row < 18; row++) {
        uint8_t blanks[3] = { UI_TILE_BLANK, UI_TILE_BLANK, UI_TILE_BLANK };
        set_win_tiles(0, row, 3, 1, blanks);
    }
}

void rth_init(void) BANKED
{
    SCX_REG = 5;
    SCY_REG = 5;
    move_win(142, 1);
    draw_border();
    draw_sidebar_layout();
    last_knowledge = 0xFFFF;
    rth_update(0, 0);
    SHOW_WIN;
}

void rth_update(const uint8_t *kind_counts, uint16_t kp) BANKED
{
    uint8_t x = KNOWLEDGE_SCREEN_X + 8;
    uint8_t y = KNOWLEDGE_SCREEN_Y + 16;
    uint8_t top, bottom;
    uint8_t r[3];
    uint8_t k;

    VBK_REG = VBK_TILES;
    /* one counter per tile kind: colored dot + two digits */
    for (k = 0; k < 4; k++) {
        uint8_t n = kind_counts ? kind_counts[k] : 0;
        if (n > 99) n = 99;
        r[0] = UI_TILE_ROUND2;   /* fill shade = the gem's body color */
        r[1] = UI_TILE_DIGIT_0 + digit_tens[n];
        r[2] = UI_TILE_DIGIT_0 + digit_ones[n];
        set_win_tiles(0, k, 3, 1, r);
    }

    /* knowledge counter sprites: K + 4 digits (timer owns the
       slots when hidden — legacy bug-2 contract) */
    if (!knowledge_visible) {
        last_knowledge = 0xFFFF;
        return;
    }
    set_sprite_tile(KNOWLEDGE_SPRITE_BASE, KNOWLEDGE_TILE_K);
    set_sprite_prop(KNOWLEDGE_SPRITE_BASE, KNOWLEDGE_PAL);
    move_sprite(KNOWLEDGE_SPRITE_BASE, x, y);

    if (kp == last_knowledge) return;
    last_knowledge = kp;

    top = (uint8_t)(kp / 100);
    bottom = (uint8_t)(kp % 100);

    set_sprite_tile(KNOWLEDGE_SPRITE_BASE + 1,
                    KNOWLEDGE_TILE_0 + digit_tens[top]);
    set_sprite_prop(KNOWLEDGE_SPRITE_BASE + 1, KNOWLEDGE_PAL);
    move_sprite(KNOWLEDGE_SPRITE_BASE + 1, x + 8, y);
    set_sprite_tile(KNOWLEDGE_SPRITE_BASE + 2,
                    KNOWLEDGE_TILE_0 + digit_ones[top]);
    set_sprite_prop(KNOWLEDGE_SPRITE_BASE + 2, KNOWLEDGE_PAL);
    move_sprite(KNOWLEDGE_SPRITE_BASE + 2, x + 16, y);
    set_sprite_tile(KNOWLEDGE_SPRITE_BASE + 3,
                    KNOWLEDGE_TILE_0 + digit_tens[bottom]);
    set_sprite_prop(KNOWLEDGE_SPRITE_BASE + 3, KNOWLEDGE_PAL);
    move_sprite(KNOWLEDGE_SPRITE_BASE + 3, x + 24, y);
    set_sprite_tile(KNOWLEDGE_SPRITE_BASE + 4,
                    KNOWLEDGE_TILE_0 + digit_ones[bottom]);
    set_sprite_prop(KNOWLEDGE_SPRITE_BASE + 4, KNOWLEDGE_PAL);
    move_sprite(KNOWLEDGE_SPRITE_BASE + 4, x + 32, y);
}

void rth_set_knowledge_visible(uint8_t visible) BANKED
{
    knowledge_visible = visible;
    if (!visible) {
        uint8_t i;
        for (i = 0; i < 5; i++) {
            move_sprite(KNOWLEDGE_SPRITE_BASE + i, 0, 0);
        }
    }
    last_knowledge = 0xFFFF;
}

void rth_show_timer(uint16_t frames_left) BANKED
{
    uint8_t secs = (uint8_t)(frames_left / 60);
    uint8_t x = 3 + 8;      /* TIMER_SCREEN_X + sprite offset  */
    uint8_t y = 134 + 16;   /* TIMER_SCREEN_Y + sprite offset  */

    set_sprite_tile(KNOWLEDGE_SPRITE_BASE, KNOWLEDGE_TILE_0 + digit_tens[secs]);
    set_sprite_prop(KNOWLEDGE_SPRITE_BASE, 0);
    move_sprite(KNOWLEDGE_SPRITE_BASE, x, y);
    set_sprite_tile(KNOWLEDGE_SPRITE_BASE + 1,
                    KNOWLEDGE_TILE_0 + digit_ones[secs]);
    set_sprite_prop(KNOWLEDGE_SPRITE_BASE + 1, 0);
    move_sprite(KNOWLEDGE_SPRITE_BASE + 1, x + 8, y);
    /* legacy bug-2: keep the stale knowledge digits hidden */
    move_sprite(KNOWLEDGE_SPRITE_BASE + 2, 0, 0);
    move_sprite(KNOWLEDGE_SPRITE_BASE + 3, 0, 0);
    move_sprite(KNOWLEDGE_SPRITE_BASE + 4, 0, 0);
}

#define OPP_SCREEN_X 117
#define OPP_SCREEN_Y 135
#define OPP_PAL 4   /* red palette loaded by battle entry */

void rth_show_opponent(uint16_t kp) BANKED
{
    uint8_t x = OPP_SCREEN_X + 8;
    uint8_t y = OPP_SCREEN_Y + 16;
    uint8_t top = (uint8_t)(kp / 100);
    uint8_t bottom = (uint8_t)(kp % 100);
    uint8_t i;
    uint8_t tiles[5];

    tiles[0] = OPP_KNOWLEDGE_TILE_C;
    tiles[1] = KNOWLEDGE_TILE_0 + digit_tens[top];
    tiles[2] = KNOWLEDGE_TILE_0 + digit_ones[top];
    tiles[3] = KNOWLEDGE_TILE_0 + digit_tens[bottom];
    tiles[4] = KNOWLEDGE_TILE_0 + digit_ones[bottom];
    for (i = 0; i < 5; i++) {
        set_sprite_tile(OPP_KNOWLEDGE_SPRITE_BASE + i, tiles[i]);
        set_sprite_prop(OPP_KNOWLEDGE_SPRITE_BASE + i, OPP_PAL);
        move_sprite(OPP_KNOWLEDGE_SPRITE_BASE + i, x + (i << 3), y);
    }
}

void rth_hide_opponent(void) BANKED
{
    uint8_t i;
    for (i = 0; i < 5; i++) {
        move_sprite(OPP_KNOWLEDGE_SPRITE_BASE + i, 0, 0);
    }
}

/* ── spell list sidebar (window rows 6-17, legacy render_spell_list
 *    geometry, ability-table driven) ─────────────────────────── */

#define LTR(c) (UI_TILE_LETTER_A + (c) - 'A')

static uint8_t spell_list_offset;

/* element palette: dominant cost, ties resolve earth > water > fire
 * (reproduces the legacy per-spell table exactly) */
static uint8_t ab_elem_pal(const ng_ability *a)
{
    if (a->cost_earth >= a->cost_water && a->cost_earth >= a->cost_fire)
        return a->cost_earth ? PAL_EARTH : PAL_SILVER;
    if (a->cost_water >= a->cost_fire) return PAL_WATER;
    return PAL_FIRE;
}

static void win_row3(uint8_t y, const uint8_t *tiles, const uint8_t *attrs)
{
    VBK_REG = VBK_ATTRIBUTES;
    set_win_tiles(0, y, 3, 1, attrs);
    VBK_REG = VBK_TILES;
    set_win_tiles(0, y, 3, 1, tiles);
}

static void cost_row(uint8_t y, uint8_t pal, uint8_t cost)
{
    uint8_t t[3] = { UI_TILE_BLANK, UI_TILE_BLANK, UI_TILE_BLANK };
    uint8_t a[3] = { PAL_SILVER, PAL_SILVER, PAL_SILVER };
    if (cost) {
        a[0] = pal;
        t[0] = UI_TILE_SMALL_DOT;
        if (cost >= 10) t[1] = UI_TILE_SMALL_DIGIT_0 + cost / 10;
        t[2] = UI_TILE_SMALL_DIGIT_0 + cost % 10;
    }
    win_row3(y, t, a);
}

void rth_spell_list(const ng_ability *tab, uint8_t tab_n,
                    uint8_t owned, uint8_t active_id,
                    const rt_engine *e, uint8_t cast_hint) BANKED
{
    uint8_t list[8];
    uint8_t n = 0, sel = 0;
    uint8_t i, id;
    uint8_t up, down;
    uint8_t t[3], at[3];

    for (id = 1; id <= tab_n && id <= 7; id++) {
        if (owned & (uint8_t)(1 << id)) {
            if (id == active_id) sel = n;
            list[n++] = id;
        }
    }
    if (sel < spell_list_offset) spell_list_offset = sel;
    if (sel > spell_list_offset + 2) spell_list_offset = sel - 2;
    up = (spell_list_offset > 0);
    down = (n > 3 && spell_list_offset < n - 3);

    /* "B:" label */
    t[0] = UI_TILE_BLANK; t[1] = LTR('B'); t[2] = UI_TILE_COLON;
    at[0] = at[1] = at[2] = PAL_SILVER;
    win_row3(6, t, at);

    /* scroll arrows */
    t[1] = up ? UI_TILE_TRI_UP : UI_TILE_TRI_UP_DIS;
    t[2] = UI_TILE_BLANK;
    at[1] = up ? PAL_SILVER : PAL_DARK;
    win_row3(7, t, at);

    for (i = 0; i < 3; i++) {
        uint8_t li = spell_list_offset + i;
        if (li < n) {
            const ng_ability *a = &tab[list[li] - 1];
            uint8_t afford = ab_can_cast(a, e);
            uint8_t icon = UI_TILE_SPARK + (list[li] - 1);
            t[0] = (li == sel) ? UI_TILE_SEL_L : UI_TILE_BLANK;
            t[1] = afford ? icon : (uint8_t)(icon + 7);
            t[2] = (li == sel) ? UI_TILE_SEL_R : UI_TILE_BLANK;
            at[0] = at[2] = PAL_SILVER;
            at[1] = afford ? ab_elem_pal(a) : PAL_DARK;
        } else {
            t[0] = t[1] = t[2] = UI_TILE_BLANK;
            at[0] = at[1] = at[2] = PAL_SILVER;
        }
        win_row3(8 + i, t, at);
    }

    t[0] = UI_TILE_BLANK;
    t[1] = down ? UI_TILE_TRI_DOWN : UI_TILE_TRI_DOWN_DIS;
    t[2] = UI_TILE_BLANK;
    at[0] = at[2] = PAL_SILVER;
    at[1] = down ? PAL_SILVER : PAL_DARK;
    win_row3(11, t, at);

    /* cost readout for the active ability (rows 12-14): one row per
     * nonzero element cost in fire, water, earth order */
    {
        uint8_t y = 12;
        if (active_id && active_id <= tab_n) {
            const ng_ability *a = &tab[active_id - 1];
            if (a->cost_fire)  cost_row(y++, PAL_FIRE, a->cost_fire);
            if (a->cost_water) cost_row(y++, PAL_WATER, a->cost_water);
            if (a->cost_earth) cost_row(y++, PAL_EARTH, a->cost_earth);
        }
        while (y <= 14) cost_row(y++, PAL_SILVER, 0);
    }

    /* "A:" cast hint (puzzle only, legacy parity) */
    t[1] = UI_TILE_COLON; t[2] = UI_TILE_BLANK;
    at[0] = at[1] = at[2] = PAL_SILVER;
    if (cast_hint) {
        t[0] = LTR('A');
    } else {
        t[0] = UI_TILE_BLANK; t[1] = UI_TILE_BLANK;
    }
    win_row3(15, t, at);
}

#define MOVES_SCREEN_X 75
#define MOVES_SCREEN_Y 135
#define MOVES_PAL_YELLOW 5
#define MOVES_PAL_RED    6

static uint8_t last_moves = 0xFF;
static uint8_t last_moves_pal = 0xFF;

/* Re-place the moves sprites from the cached value after another
   system borrowed their OAM slots (ghost hint bracket B, the hint
   wiggle). No-op when the counter is hidden. */
void rth_moves_restore(void) BANKED
{
    uint8_t m = last_moves;
    if (m == 0xFF) return;
    last_moves = 0xFE;        /* bust the cache */
    last_moves_pal = 0xFF;
    rth_show_moves(m);
}

void rth_show_moves(uint8_t moves) BANKED
{
    uint8_t pal_state, spr_pal;
    uint8_t x = MOVES_SCREEN_X + 8;
    uint8_t y = MOVES_SCREEN_Y + 16;

    if (moves == 0xFF) {
        uint8_t i;
        for (i = 0; i < 4; i++)
            move_sprite(HINT_SPRITE_BASE + i, 0, 0);
        last_moves = 0xFF;
        last_moves_pal = 0xFF;
        return;
    }

    if (moves < 2) pal_state = 2;
    else if (moves < 5) pal_state = 1;
    else pal_state = 0;

    if (moves == last_moves && pal_state == last_moves_pal) return;
    last_moves = moves;
    last_moves_pal = pal_state;

    if (pal_state == 2) {
        uint16_t pal_r[4] = { 0, 0x7FFF,
                              (uint16_t)(24 | (24 << 5) | (24 << 10)),
                              (uint16_t)(31 | (2 << 5) | (2 << 10)) };
        set_sprite_palette(MOVES_PAL_RED, 1, pal_r);
        spr_pal = MOVES_PAL_RED;
    } else if (pal_state == 1) {
        uint16_t pal_y[4] = { 0, 0x7FFF,
                              (uint16_t)(24 | (24 << 5) | (24 << 10)),
                              (uint16_t)(28 | (26 << 5)) };
        set_sprite_palette(MOVES_PAL_YELLOW, 1, pal_y);
        spr_pal = MOVES_PAL_YELLOW;
    } else {
        spr_pal = 0;
    }

    if (moves > 99) moves = 99;
    set_sprite_tile(HINT_SPRITE_BASE, MOVES_SPR_TILE_M);
    set_sprite_prop(HINT_SPRITE_BASE, 0);
    move_sprite(HINT_SPRITE_BASE, x, y);
    set_sprite_tile(HINT_SPRITE_BASE + 1, MOVES_SPR_TILE_V);
    set_sprite_prop(HINT_SPRITE_BASE + 1, 0);
    move_sprite(HINT_SPRITE_BASE + 1, x + 8, y);
    set_sprite_tile(HINT_SPRITE_BASE + 2, MOVES_SPR_TILE_D0 + digit_tens[moves]);
    set_sprite_prop(HINT_SPRITE_BASE + 2, spr_pal);
    move_sprite(HINT_SPRITE_BASE + 2, x + 16, y);
    set_sprite_tile(HINT_SPRITE_BASE + 3, MOVES_SPR_TILE_D0 + digit_ones[moves]);
    set_sprite_prop(HINT_SPRITE_BASE + 3, spr_pal);
    move_sprite(HINT_SPRITE_BASE + 3, x + 24, y);
}
