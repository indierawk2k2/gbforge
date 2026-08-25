#include "gbf_input.h"
#include "gbf_symbols.h"
#include "gb.h"
#include <stdio.h>

/* ── Low-level input ───────────────────────────────── */

void harness_press(GbfHarness *h, unsigned key)
{
    if (key < HARNESS_KEY_MAX) {
        GB_set_key_state(h->gb, (GB_key_t)key, true);
    }
}

void harness_release(GbfHarness *h, unsigned key)
{
    if (key < HARNESS_KEY_MAX) {
        GB_set_key_state(h->gb, (GB_key_t)key, false);
    }
}

void harness_tap(GbfHarness *h, unsigned key, uint32_t hold_frames)
{
    if (hold_frames == 0) hold_frames = 2;

    harness_press(h, key);
    harness_run_frames(h, hold_frames);
    harness_release(h, key);
    harness_run_frames(h, 2); /* Brief release gap */
}

void harness_release_all(GbfHarness *h)
{
    for (unsigned k = 0; k < HARNESS_KEY_MAX; k++) {
        GB_set_key_state(h->gb, (GB_key_t)k, false);
    }
}

/* ── High-level navigation ─────────────────────────── */

void harness_navigate_to(GbfHarness *h, uint8_t x, uint8_t y)
{
#if defined(ADDR_CURSOR_X) && defined(ADDR_CURSOR_Y)
    /* Read current cursor position from game memory */
    uint8_t cx = harness_read_byte(h, ADDR_CURSOR_X);
    uint8_t cy = harness_read_byte(h, ADDR_CURSOR_Y);

    /* Move horizontally */
    while (cx != x) {
        if (cx < x) {
            harness_tap(h, HARNESS_KEY_RIGHT, 2);
            cx++;
        } else {
            harness_tap(h, HARNESS_KEY_LEFT, 2);
            cx--;
        }
        /* Wait for cursor animation to settle */
        harness_run_frames(h, 4);
    }

    /* Move vertically */
    while (cy != y) {
        if (cy < y) {
            harness_tap(h, HARNESS_KEY_DOWN, 2);
            cy++;
        } else {
            harness_tap(h, HARNESS_KEY_UP, 2);
            cy--;
        }
        harness_run_frames(h, 4);
    }
#else
    (void)h; (void)x; (void)y;
    fprintf(stderr, "harness: navigate_to requires ADDR_CURSOR_X/Y symbols\n");
#endif
}

int harness_wait_cascade(GbfHarness *h, uint32_t max_frames)
{
#ifdef ADDR_PROCESSING_MATCHES
    for (uint32_t i = 0; i < max_frames; i++) {
        GB_run_frame(h->gb);
        if (harness_read_byte(h, ADDR_PROCESSING_MATCHES) == 0) {
            return 1;
        }
    }
    fprintf(stderr, "harness: cascade didn't complete within %u frames\n",
            max_frames);
    return 0;
#else
    /* Without symbol, just wait the full duration */
    harness_run_frames(h, max_frames);
    return 1;
#endif
}

void harness_select_and_swap(GbfHarness *h,
                              uint8_t x1, uint8_t y1,
                              uint8_t x2, uint8_t y2)
{
    /* Navigate to source tile */
    harness_navigate_to(h, x1, y1);

    /* Select (tap A to grab) */
    harness_tap(h, HARNESS_KEY_A, 2);
    harness_run_frames(h, 4);

    /* Press d-pad toward target */
    if (x2 > x1) harness_tap(h, HARNESS_KEY_RIGHT, 2);
    else if (x2 < x1) harness_tap(h, HARNESS_KEY_LEFT, 2);
    else if (y2 > y1) harness_tap(h, HARNESS_KEY_DOWN, 2);
    else if (y2 < y1) harness_tap(h, HARNESS_KEY_UP, 2);

    /* Wait for swap animation + cascade to complete */
    harness_wait_cascade(h, 600); /* 10 seconds at 60fps */
}
