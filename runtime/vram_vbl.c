/* gbforge runtime — the staged-blast DMA, executed inside the VBlank
 * interrupt. HOME-resident on purpose (ISR handlers run with any ROM
 * bank mapped).
 *
 * The main loop stages rows into the shadow (vram_dma.c), then
 * rtv_blast() arms this handler and waits for vblank. Installed ahead
 * of the sound driver (rtv_blast_install() before sound init), the
 * handler runs right after the OAM DMA, at LY ~144.3, so even a
 * whole-board blast (1 KB, ~4.5 scanlines) lands well inside vblank.
 * Blasting from the main loop after vsync() could only start after
 * the sound ISR (~3 lines) and large blasts then had to give up a
 * whole frame — the last source of "+N" float stalls. */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "vram.h"

/* set by rtv_blast (bank 3), consumed here */
volatile uint8_t rtv_vbl_armed;
const uint8_t *rtv_vbl_src_attr;
const uint8_t *rtv_vbl_src_tile;
uint16_t rtv_vbl_dst;
uint8_t rtv_vbl_blocks;

static void gdma16(const uint8_t *src, uint16_t dst, uint8_t blocks)
{
    HDMA1_REG = (uint8_t)((uint16_t)src >> 8);
    HDMA2_REG = (uint8_t)src;
    HDMA3_REG = (uint8_t)(dst >> 8);
    HDMA4_REG = (uint8_t)dst;
    HDMA5_REG = (uint8_t)(blocks - 1);      /* GDMA; CPU stalls until done */
}

void rtv_blast_vbl(void)
{
    uint8_t vbk;
    if (!rtv_vbl_armed) return;
    vbk = VBK_REG;
    VBK_REG = VBK_ATTRIBUTES;
    gdma16(rtv_vbl_src_attr, rtv_vbl_dst, rtv_vbl_blocks);
    VBK_REG = VBK_TILES;
    gdma16(rtv_vbl_src_tile, rtv_vbl_dst, rtv_vbl_blocks);
    VBK_REG = vbk;
    rtv_vbl_armed = 0;
}

/* Direct copy for the LCD-off case (no vblank interrupts then). */
void rtv_blast_now(void)
{
    rtv_blast_vbl();
}

void rtv_blast_install(void)
{
    rtv_vbl_armed = 0;
    add_VBL(rtv_blast_vbl);
}
