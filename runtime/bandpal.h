#ifndef BANDPAL_H_INCLUDED
#define BANDPAL_H_INCLUDED

/* Per-band BG palette streaming — see bandpal.c. All functions are
 * NONBANKED (HOME): callable from any bank without trampolines.
 *
 * Contract for callers:
 *  - screen SCY must be 0 and the image tile-row aligned (y0 = bg
 *    tile row of the image's first band);
 *  - bands alternate palette slots 0-2 / 3-5; slot 6 is left alone,
 *    slot 7 stays the UI palette;
 *  - with `restore`, the bg rows directly ABOVE and BELOW the
 *    image must be blank or UI-palette (bands 0/1 stream into the
 *    row above; the last restore lands in the row below), and the
 *    image may not start at bg row 0;
 *  - call bandpal_stop() before leaving the screen. */

#include <stdint.h>
#include <gbdk/platform.h>

void bandpal_start(uint8_t bank, const uint16_t *pals, uint8_t bands,
                   uint8_t y0, const uint16_t *restore) BANKED;
void bandpal_stop(void) BANKED;
void bandpal_blit_row(uint8_t bank, const uint8_t *tiles,
                      const uint8_t *attrs, uint8_t x, uint8_t y,
                      uint8_t w, uint8_t row, const uint8_t *vmap,
                      uint8_t vbase, uint8_t vbk1) BANKED;
void bandpal_blit(uint8_t bank, const uint8_t *tiles,
                  const uint8_t *attrs, uint8_t x, uint8_t y,
                  uint8_t w, uint8_t bands, const uint8_t *vmap,
                  uint8_t vbase, uint8_t vbk1) BANKED;

#endif
