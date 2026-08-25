/* gbctl — headless automation oracle for the gbforge verification loop.
 *
 * Links libsameboy via the existing harness core and speaks the shared
 * line protocol (see harness/PROTOCOL.md) over stdin/stdout: one command
 * line in, one JSON reply line out. Emulation is parked by design —
 * frames advance only on run_frames/step/tap/run_to_vblank — so runs
 * are deterministic and CI-safe (no window, no wall clock).
 *
 * Usage: gbctl <rom.gbc> [boot_rom.bin]
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gbf_harness.h"
#include "gbf_input.h"
#include "gb.h"

#define MAX_LINE 4096
#define BLOCK_CAP 256

static GbfHarness g_h;
static const char *g_boot_rom;

/* Run the APU like a real host (sample rate + discard callback).
 * Without this, savestates carry APU state from a silent core that
 * trips the sample-timing assertion in audio-enabled builds (the
 * Cocoa fork) when they load our states. */
static void discard_sample(GB_gameboy_t *gb, GB_sample_t *sample)
{
    (void)gb;
    (void)sample;
}

static void enable_apu_host(void)
{
    GB_set_sample_rate(g_h.gb, 44100);
    GB_apu_set_sample_callback(g_h.gb, discard_sample);
}

/* ── Reply helpers ─────────────────────────────────── */

static void reply(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
    fflush(stdout);
}

static void reply_err(const char *msg)
{
    reply("{\"ok\":false,\"error\":\"%s\"}", msg);
}

/* ── Parsing helpers ───────────────────────────────── */

static int parse_u32(const char *s, uint32_t *out)
{
    if (!s || !*s) return 1;
    *out = (uint32_t)strtoul(s, NULL, 0); /* handles 0x prefix */
    return 0;
}

static int button_key(const char *name)
{
    static const char *names[HARNESS_KEY_MAX] = {
        "RIGHT", "LEFT", "UP", "DOWN", "A", "B", "SELECT", "START"
    };
    for (int i = 0; i < HARNESS_KEY_MAX; i++) {
        if (strcmp(name, names[i]) == 0) return i;
    }
    return -1;
}

/* ── Base64 (for screenshot_raw) ───────────────────── */

static void base64_encode(const uint8_t *src, size_t len, char *dst)
{
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i = 0, o = 0;
    while (i + 2 < len) {
        uint32_t v = (src[i] << 16) | (src[i + 1] << 8) | src[i + 2];
        dst[o++] = tbl[(v >> 18) & 63];
        dst[o++] = tbl[(v >> 12) & 63];
        dst[o++] = tbl[(v >> 6) & 63];
        dst[o++] = tbl[v & 63];
        i += 3;
    }
    if (i + 1 == len) {
        uint32_t v = src[i] << 16;
        dst[o++] = tbl[(v >> 18) & 63];
        dst[o++] = tbl[(v >> 12) & 63];
        dst[o++] = '=';
        dst[o++] = '=';
    } else if (i + 2 == len) {
        uint32_t v = (src[i] << 16) | (src[i + 1] << 8);
        dst[o++] = tbl[(v >> 18) & 63];
        dst[o++] = tbl[(v >> 12) & 63];
        dst[o++] = tbl[(v >> 6) & 63];
        dst[o++] = '=';
    }
    dst[o] = '\0';
}

/* Harness pixels are XRGB8888 (r<<16 | g<<8 | b). Wire format is
   RGBA8888 bytes, row-major, top-left (PROTOCOL.md). */
static void pixels_to_rgba(const uint32_t *px, uint8_t *out)
{
    for (int i = 0; i < GB_PIXEL_COUNT; i++) {
        out[i * 4 + 0] = (px[i] >> 16) & 0xFF;
        out[i * 4 + 1] = (px[i] >> 8) & 0xFF;
        out[i * 4 + 2] = px[i] & 0xFF;
        out[i * 4 + 3] = 0xFF;
    }
}

/* ── Command dispatch ──────────────────────────────── */

static int handle_line(char *line)
{
    char *argv[8];
    int argc = 0;
    for (char *tok = strtok(line, " \t\r\n");
         tok && argc < 8;
         tok = strtok(NULL, " \t\r\n")) {
        argv[argc++] = tok;
    }
    if (argc == 0) return 0;
    const char *cmd = argv[0];

    /* ── Execution ── */
    if (strcmp(cmd, "run_frames") == 0) {
        uint32_t n;
        if (argc < 2 || parse_u32(argv[1], &n)) { reply_err("Usage: run_frames <n>"); return 0; }
        harness_run_frames(&g_h, n);
        reply("{\"ok\":true,\"frames\":%u}", g_h.frame_count);
        return 0;
    }
    if (strcmp(cmd, "step") == 0 || strcmp(cmd, "run_to_vblank") == 0) {
        harness_run_frames(&g_h, 1);
        reply("{\"ok\":true,\"frames\":%u}", g_h.frame_count);
        return 0;
    }
    if (strcmp(cmd, "pause") == 0 || strcmp(cmd, "resume") == 0) {
        reply("{\"ok\":true}"); /* always parked in headless mode */
        return 0;
    }
    if (strcmp(cmd, "set_speed") == 0) {
        reply("{\"ok\":true}"); /* headless runs unthrottled */
        return 0;
    }
    if (strcmp(cmd, "quit") == 0) {
        reply("{\"ok\":true}");
        return 1;
    }

    /* ── Input ── */
    if (strcmp(cmd, "press") == 0 || strcmp(cmd, "release") == 0) {
        if (argc < 2) { reply_err("Usage: press|release <btn>"); return 0; }
        int key = button_key(argv[1]);
        if (key < 0) { reply_err("Unknown button"); return 0; }
        if (cmd[0] == 'p') harness_press(&g_h, (unsigned)key);
        else harness_release(&g_h, (unsigned)key);
        reply("{\"ok\":true}");
        return 0;
    }
    if (strcmp(cmd, "tap") == 0) {
        if (argc < 2) { reply_err("Usage: tap <btn> [hold]"); return 0; }
        int key = button_key(argv[1]);
        if (key < 0) { reply_err("Unknown button"); return 0; }
        uint32_t hold = 2;
        if (argc >= 3) parse_u32(argv[2], &hold);
        harness_tap(&g_h, (unsigned)key, hold);
        reply("{\"ok\":true,\"frames\":%u}", g_h.frame_count);
        return 0;
    }

    /* ── Memory ── */
    if (strcmp(cmd, "read") == 0) {
        uint32_t addr;
        if (argc < 2 || parse_u32(argv[1], &addr) || addr > 0xFFFF) {
            reply_err("Usage: read <addr>"); return 0;
        }
        reply("{\"ok\":true,\"value\":%u}", harness_read_byte(&g_h, (uint16_t)addr));
        return 0;
    }
    if (strcmp(cmd, "regs") == 0) {
        GB_registers_t *r = GB_get_registers(g_h.gb);
        size_t rom_size; uint16_t bank = 0;
        GB_get_direct_access(g_h.gb, GB_DIRECT_ACCESS_ROM, &rom_size, &bank);
        reply("{\"ok\":true,\"pc\":%u,\"sp\":%u,\"bank\":%u}",
              r->pc, r->sp, bank);
        return 0;
    }
    if (strcmp(cmd, "read_block") == 0) {
        uint32_t addr, count;
        if (argc < 3 || parse_u32(argv[1], &addr) || parse_u32(argv[2], &count) ||
            addr > 0xFFFF || count == 0 || count > BLOCK_CAP) {
            reply_err("Usage: read_block <addr> <count<=256>"); return 0;
        }
        uint8_t buf[BLOCK_CAP];
        harness_read_block(&g_h, (uint16_t)addr, buf, count);
        char hex[BLOCK_CAP * 2 + 1];
        for (uint32_t i = 0; i < count; i++) sprintf(hex + i * 2, "%02X", buf[i]);
        reply("{\"ok\":true,\"data\":\"%s\"}", hex);
        return 0;
    }
    if (strcmp(cmd, "write_byte") == 0) {
        uint32_t addr, val;
        if (argc < 3 || parse_u32(argv[1], &addr) || parse_u32(argv[2], &val) ||
            addr > 0xFFFF || val > 0xFF) {
            reply_err("Usage: write_byte <addr> <val>"); return 0;
        }
        GB_write_memory(g_h.gb, (uint16_t)addr, (uint8_t)val);
        reply("{\"ok\":true}");
        return 0;
    }
    if (strcmp(cmd, "write_block") == 0) {
        uint32_t addr;
        if (argc < 3 || parse_u32(argv[1], &addr) || addr > 0xFFFF) {
            reply_err("Usage: write_block <addr> <hex>"); return 0;
        }
        const char *hex = argv[2];
        size_t hlen = strlen(hex);
        if (hlen == 0 || hlen % 2 || hlen / 2 > BLOCK_CAP) {
            reply_err("hex payload must be 1-256 bytes"); return 0;
        }
        for (size_t i = 0; i < hlen / 2; i++) {
            unsigned byte;
            if (sscanf(hex + i * 2, "%2x", &byte) != 1) {
                reply_err("bad hex"); return 0;
            }
            GB_write_memory(g_h.gb, (uint16_t)(addr + i), (uint8_t)byte);
        }
        reply("{\"ok\":true,\"count\":%zu}", hlen / 2);
        return 0;
    }
    if (strcmp(cmd, "read_vram") == 0) {
        uint32_t bank, addr, count;
        if (argc < 4 || parse_u32(argv[1], &bank) || parse_u32(argv[2], &addr) ||
            parse_u32(argv[3], &count) || bank > 1 ||
            addr < 0x8000 || addr > 0x9FFF || count == 0 || count > BLOCK_CAP) {
            reply_err("Usage: read_vram <bank 0|1> <addr 0x8000-0x9FFF> <count<=256>");
            return 0;
        }
        size_t size; uint16_t cur_bank;
        uint8_t *vram = GB_get_direct_access(g_h.gb, GB_DIRECT_ACCESS_VRAM,
                                             &size, &cur_bank);
        if (!vram) { reply_err("VRAM direct access failed"); return 0; }
        size_t off = bank * 0x2000 + (addr - 0x8000);
        char hex[BLOCK_CAP * 2 + 1];
        for (uint32_t i = 0; i < count; i++) {
            uint8_t b = (off + i < size) ? vram[off + i] : 0xFF;
            sprintf(hex + i * 2, "%02X", b);
        }
        reply("{\"ok\":true,\"data\":\"%s\"}", hex);
        return 0;
    }
    if (strcmp(cmd, "snapshot") == 0) {
        uint32_t addr, w, h;
        if (argc < 4 || parse_u32(argv[1], &addr) || parse_u32(argv[2], &w) ||
            parse_u32(argv[3], &h) || addr > 0xFFFF || w == 0 || h == 0 ||
            w * h > BLOCK_CAP) {
            reply_err("Usage: snapshot <addr> <w> <h> (w*h<=256)"); return 0;
        }
        char buf[BLOCK_CAP * 4 + 64];
        char *p = buf;
        p += sprintf(p, "{\"ok\":true,\"rows\":[");
        for (uint32_t y = 0; y < h; y++) {
            p += sprintf(p, "%s[", y ? "," : "");
            for (uint32_t x = 0; x < w; x++) {
                uint8_t v = harness_read_byte(&g_h, (uint16_t)(addr + y * w + x));
                p += sprintf(p, "%s%u", x ? "," : "", v);
            }
            p += sprintf(p, "]");
        }
        sprintf(p, "]}");
        reply("%s", buf);
        return 0;
    }

    /* ── PPU direct access ── */
    if (strcmp(cmd, "read_obj_palette") == 0 || strcmp(cmd, "read_bg_palette") == 0) {
        uint32_t slot, color;
        if (argc < 3 || parse_u32(argv[1], &slot) || parse_u32(argv[2], &color) ||
            slot > 7 || color > 3) {
            reply_err("Usage: read_*_palette <slot 0-7> <color 0-3>"); return 0;
        }
        GB_direct_access_t which = (cmd[5] == 'o') ? GB_DIRECT_ACCESS_OBP
                                                   : GB_DIRECT_ACCESS_BGP;
        size_t size; uint16_t bank;
        uint8_t *pal = GB_get_direct_access(g_h.gb, which, &size, &bank);
        if (!pal) { reply_err("palette direct access failed"); return 0; }
        size_t off = (slot * 4 + color) * 2;
        uint16_t raw = pal[off] | (pal[off + 1] << 8);
        reply("{\"ok\":true,\"r\":%u,\"g\":%u,\"b\":%u}",
              raw & 0x1F, (raw >> 5) & 0x1F, (raw >> 10) & 0x1F);
        return 0;
    }
    if (strcmp(cmd, "read_oam") == 0) {
        uint32_t slot;
        if (argc < 2 || parse_u32(argv[1], &slot) || slot > 39) {
            reply_err("Usage: read_oam <slot 0-39>"); return 0;
        }
        size_t size; uint16_t bank;
        uint8_t *oam = GB_get_direct_access(g_h.gb, GB_DIRECT_ACCESS_OAM,
                                            &size, &bank);
        if (!oam) { reply_err("OAM direct access failed"); return 0; }
        reply("{\"ok\":true,\"y\":%u,\"x\":%u,\"tile\":%u,\"attr\":%u}",
              oam[slot * 4], oam[slot * 4 + 1], oam[slot * 4 + 2], oam[slot * 4 + 3]);
        return 0;
    }

    /* ── Savestates ── */
    if (strcmp(cmd, "save_state") == 0 || strcmp(cmd, "load_state") == 0) {
        if (argc < 2) { reply_err("Usage: save_state|load_state <path>"); return 0; }
        int rc = (cmd[0] == 's') ? GB_save_state(g_h.gb, argv[1])
                                 : GB_load_state(g_h.gb, argv[1]);
        if (rc == 0) reply("{\"ok\":true,\"path\":\"%s\"}", argv[1]);
        else reply_err("state operation failed");
        return 0;
    }

    /* ── Capture ──
     *
     * Only the raw framebuffer crosses the wire. PNG encoding lives
     * in pygb (stdlib zlib), so there is exactly one encoder to keep
     * correct and the harness links no image library. */
    if (strcmp(cmd, "screenshot_raw") == 0) {
        static uint8_t rgba[GB_PIXEL_COUNT * 4];
        static char b64[(GB_PIXEL_COUNT * 4 + 2) / 3 * 4 + 8];
        pixels_to_rgba(g_h.pixels, rgba);
        base64_encode(rgba, sizeof(rgba), b64);
        printf("{\"ok\":true,\"w\":160,\"h\":144,\"format\":\"rgba8888\",\"data\":\"%s\"}\n",
               b64);
        fflush(stdout);
        return 0;
    }

    /* ── ROM ── */
    if (strcmp(cmd, "load_rom") == 0) {
        if (argc < 2) { reply_err("Usage: load_rom <path>"); return 0; }
        char rom[512];
        snprintf(rom, sizeof(rom), "%s", argv[1]);
        harness_free(&g_h);
        if (harness_init_with_paths(&g_h, rom, g_boot_rom)) {
            reply_err("load_rom failed");
            return 1; /* no emulator left to drive */
        }
        enable_apu_host();
        reply("{\"ok\":true,\"rom\":\"%s\"}", rom);
        return 0;
    }

    reply_err("Unknown command");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom.gbc> [boot_rom.bin]\n", argv[0]);
        return 1;
    }
    g_boot_rom = (argc >= 3) ? argv[2] : BOOT_ROM_PATH;

    if (harness_init_with_paths(&g_h, argv[1], g_boot_rom)) {
        return 1;
    }
    enable_apu_host();

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), stdin)) {
        if (handle_line(line)) break;
    }
    harness_free(&g_h);
    return 0;
}
