#ifndef GBFORGE_HUD_H_INCLUDED
#define GBFORGE_HUD_H_INCLUDED

/* gbforge runtime — the standard game HUD (legacy layout parity):
 * bordered board frame, window-layer manna sidebar (sub-pixel-shifted
 * digit tile variants from the res contract), sprite-based knowledge
 * counter. Screen geometry matches legacy render_init exactly:
 * SCX/SCY=5, board pixels at (3,3), window at WX=142.
 */

#include <stdint.h>

#include "ability.h"
#include "engine.h"

#ifdef __SDCC
#include <asm/types.h>
#else
#ifndef BANKED
#define BANKED
#endif
#endif

/* Legacy screen geometry: sprites position against these. */
#define RT_SCREEN_OFF_X 3
#define RT_SCREEN_OFF_Y 3

/* Full HUD bring-up: scroll, border, window layout, knowledge zeroes.
 * Call between DISPLAY_OFF/ON with tiles already loaded. */
void rth_init(void) BANKED;

/* Sidebar refresh: four per-kind counters (kind_counts[0..3],
 * NULL = zeros) and the score readout. */
void rth_update(const uint8_t *kind_counts, uint16_t kp) BANKED;

/* Timed modes reuse the knowledge sprite slots for the countdown:
 * hide the counter first, then feed frames-left each tick. */
void rth_set_knowledge_visible(uint8_t visible) BANKED;
void rth_show_timer(uint16_t frames_left) BANKED;

/* Battle opponent counter (C + 4 digits, sprites 20-24). */
void rth_show_opponent(uint16_t kp) BANKED;
void rth_hide_opponent(void) BANKED;

/* Available-moves counter (legacy endless HUD: MV + 2 digits on
 * sprites 36-39, yellow < 5, red < 2). 0xFF hides it. */
void rth_show_moves(uint8_t moves) BANKED;

/* Spell list sidebar (window rows 6-17): owned abilities with
 * selection brackets, affordability palettes, active-cost rows. */
void rth_spell_list(const ng_ability *tab, uint8_t tab_n,
                    uint8_t owned, uint8_t active_id,
                    const rt_engine *e, uint8_t cast_hint) BANKED;

#endif
