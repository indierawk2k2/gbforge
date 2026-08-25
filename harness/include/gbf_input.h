#ifndef GBF_INPUT_H
#define GBF_INPUT_H

#include "gbf_harness.h"

/* SameBoy key constants (matches GB_key_t enum) */
#define HARNESS_KEY_RIGHT  0
#define HARNESS_KEY_LEFT   1
#define HARNESS_KEY_UP     2
#define HARNESS_KEY_DOWN   3
#define HARNESS_KEY_A      4
#define HARNESS_KEY_B      5
#define HARNESS_KEY_SELECT 6
#define HARNESS_KEY_START  7
#define HARNESS_KEY_MAX    8

/* ── Low-level input ───────────────────────────────── */

/* Press and hold a button (stays pressed until released). */
void harness_press(GbfHarness *h, unsigned key);

/* Release a button. */
void harness_release(GbfHarness *h, unsigned key);

/* Tap a button: press for hold_frames, release, wait release_frames.
   Default: hold 2 frames, release wait 2 frames. */
void harness_tap(GbfHarness *h, unsigned key, uint32_t hold_frames);

/* Release all buttons. */
void harness_release_all(GbfHarness *h);

/* ── High-level navigation ─────────────────────────── */

/* Move cursor from its current position to target (x, y).
   Reads cursor_x/cursor_y from game memory and presses d-pad
   the needed number of times, waiting for animation between presses. */
void harness_navigate_to(GbfHarness *h, uint8_t x, uint8_t y);

/* Select tile at (x1,y1), swap toward (x2,y2).
   Navigates to (x1,y1), taps A, presses d-pad toward (x2,y2),
   then waits for processing_matches to return to 0. */
void harness_select_and_swap(GbfHarness *h,
                              uint8_t x1, uint8_t y1,
                              uint8_t x2, uint8_t y2);

/* Wait for processing_matches to become 0 (cascade complete).
   Returns 1 if cleared within max_frames, 0 on timeout. */
int harness_wait_cascade(GbfHarness *h, uint32_t max_frames);

#endif /* GBF_INPUT_H */
