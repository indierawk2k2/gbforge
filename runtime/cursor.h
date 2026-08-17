#ifndef GBFORGE_CURSOR_H_INCLUDED
#define GBFORGE_CURSOR_H_INCLUDED

/* gbforge runtime — the four-corner L-bracket cursor (exact legacy
 * render_cursor + cursor_animate port): pixel glide at 6 px/frame,
 * mode-driven tiles and dynamic palettes.
 *
 * Modes (legacy contract):
 *   0 normal (white)          1 selected (gray, 1px inset)
 *   2 spell targeting (flash) 3 slide 1st target (blue flash)
 *   4 slide 2nd target (red flash)
 *   5 opponent (solid red)    6 opponent grabbed (dark red, inset)
 */

#include <stdint.h>

#ifdef __SDCC
#include <asm/types.h>
#else
#ifndef BANKED
#define BANKED
#endif
#endif

/* pixel position, glided toward (cursor grid << 4) — the legacy
 * cursor_px/cursor_py contract for the harness */
extern int16_t cursor_px, cursor_py;

/* Snap the pixel position to a grid cell (mode entry / warps). */
void rtc_snap(uint8_t gx, uint8_t gy) BANKED;

/* One glide step toward (gx, gy) << 4. Returns 1 while moving. */
uint8_t rtc_animate(uint8_t gx, uint8_t gy) BANKED;

/* Draw the bracket at the current pixel position. frame drives the
 * targeting flash; mode as documented above. */
void rtc_draw(uint8_t frame, uint8_t mode) BANKED;

/* Invalidate caches (call after DISPLAY_OFF screen rebuilds). */
void rtc_invalidate(void) BANKED;

#endif
