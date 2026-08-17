#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

/* Debug mailbox — DEV BUILDS ONLY (make DEBUG=1, -DDEBUG_BUILD).
 *
 * The verification harness pokes these WRAM variables over the
 * automation protocol (write_block at the symbol addresses from the
 * debug build's .noi) and the game consumes them at safe points:
 *
 *   DBG_ENTER_MODE    consumed in the title-screen wait loop; jumps
 *                     straight into a mode without menu navigation.
 *                     Modes 3-5 (timed/battle/practice) are handled by
 *                     main() since they are not title-reachable.
 *   DBG_TRIGGER_SWAP  consumed in input_update(); teleports the cursor
 *                     and returns the swap event, so the active mode's
 *                     own handler runs it exactly like a player swap.
 *   DBG_REDRAW        consumed in input_update(); full board redraw
 *                     after the harness rewrites board[][] directly.
 *   debug_rng_force   checked by board.c seed_rng(); overrides the
 *                     DIV_REG seed for deterministic boards.
 *
 * Everything here compiles out of retail builds — this header is
 * self-guarded and debug.c is empty without DEBUG_BUILD.
 */

#ifdef DEBUG_BUILD

#include <stdint.h>

#define DBG_NONE          0
#define DBG_ENTER_MODE    1
#define DBG_TRIGGER_SWAP  2
#define DBG_REDRAW        3
#define DBG_CAST          4   /* ng only: cast active ability at (arg0, arg1) */

/* DBG_ENTER_MODE arg0 — 0-2 match render_title_screen return codes */
#define DBG_MODE_QUEST    0
#define DBG_MODE_PUZZLE   1
#define DBG_MODE_ENDLESS  2
#define DBG_MODE_TIMED    3   /* arg1 = target tile, arg2 = seconds   */
#define DBG_MODE_BATTLE   4   /* arg1|arg2<<8 = target knowledge (LE) */
#define DBG_MODE_PRACTICE 5   /* arg1 = max tier                      */

/* DBG_TRIGGER_SWAP: arg0 = x, arg1 = y, arg2 = dir (0 up, 1 down,
   2 left, 3 right) */

extern volatile uint8_t debug_req;
extern volatile uint8_t debug_arg0;
extern volatile uint8_t debug_arg1;
extern volatile uint8_t debug_arg2;

extern volatile uint8_t debug_phase;
extern volatile uint8_t debug_rng_force;
extern volatile uint16_t debug_rng_seed;

#endif /* DEBUG_BUILD */

#endif /* DEBUG_H_INCLUDED */
