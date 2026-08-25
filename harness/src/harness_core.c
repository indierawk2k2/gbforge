#include "gbf_harness.h"
#include "gbf_symbols.h"
#include "gb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Test tracking globals ─────────────────────────── */

int _harness_test_pass = 0;
int _harness_test_fail = 0;
int _harness_test_current_failed = 0;
const char *_harness_test_current_name = NULL;

/* ── Callbacks ─────────────────────────────────────── */

static uint32_t rgb_encode(GB_gameboy_t *gb, uint8_t r, uint8_t g, uint8_t b)
{
    (void)gb;
    /* XRGB8888: alpha channel unused, set to 0xFF */
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void vblank_callback(GB_gameboy_t *gb, GB_vblank_type_t type)
{
    (void)type;
    GbfHarness *h = (GbfHarness *)GB_get_user_data(gb);
    if (h) {
        h->frame_count++;
    }
}

static void log_callback(GB_gameboy_t *gb, const char *string, GB_log_attributes_t attributes)
{
    (void)gb;
    (void)attributes;
    /* Suppress SameBoy log spam during automated runs.
       Uncomment for debugging: */
    /* fprintf(stderr, "[SameBoy] %s", string); */
    (void)string;
}

/* ── Initialization ────────────────────────────────── */

static int harness_init_internal(GbfHarness *h,
                                  const char *rom_path,
                                  const char *boot_rom_path)
{
    memset(h, 0, sizeof(*h));
    h->rom_path = rom_path;
    h->boot_rom_path = boot_rom_path;

    h->gb = malloc(sizeof(GB_gameboy_t));
    if (!h->gb) {
        fprintf(stderr, "harness: failed to allocate GB_gameboy_t\n");
        return 1;
    }

    GB_init(h->gb, GB_MODEL_CGB_E);
    GB_set_user_data(h->gb, h);

    /* Load boot ROM (runs the Nintendo logo animation) */
    if (GB_load_boot_rom(h->gb, boot_rom_path)) {
        fprintf(stderr, "harness: failed to load boot ROM: %s\n", boot_rom_path);
        /* Not fatal — SameBoy can run without boot ROM */
    }

    /* Load the game ROM */
    if (GB_load_rom(h->gb, rom_path)) {
        fprintf(stderr, "harness: failed to load ROM: %s\n", rom_path);
        free(h->gb);
        h->gb = NULL;
        return 1;
    }

    /* Configure pixel output. Color correction is pinned explicitly —
       the library default changed across SameBoy versions (older cores
       corrected by default, current ones don't), which silently skewed
       golden comparisons when scenario binaries were relinked. Modern
       Balanced matches the Cocoa fork's rendering and the committed
       goldens. */
    GB_set_pixels_output(h->gb, h->pixels);
    GB_set_rgb_encode_callback(h->gb, rgb_encode);
    GB_set_color_correction_mode(h->gb, GB_COLOR_CORRECTION_MODERN_BALANCED);

    /* Configure callbacks */
    GB_set_vblank_callback(h->gb, vblank_callback);
    GB_set_log_callback(h->gb, log_callback);

    printf("harness: loaded ROM %s (%u frames elapsed)\n",
           rom_path, h->frame_count);
    return 0;
}

int harness_init(GbfHarness *h)
{
    return harness_init_internal(h, ROM_PATH, BOOT_ROM_PATH);
}

int harness_init_with_paths(GbfHarness *h,
                            const char *rom_path,
                            const char *boot_rom_path)
{
    return harness_init_internal(h, rom_path, boot_rom_path);
}

void harness_free(GbfHarness *h)
{
    if (h->gb) {
        GB_free(h->gb);
        free(h->gb);
        h->gb = NULL;
    }
}

/* ── Frame execution ───────────────────────────────── */

void harness_run_frames(GbfHarness *h, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        GB_run_frame(h->gb);
    }
}

int harness_run_until(GbfHarness *h, uint32_t max_frames,
                      int (*pred)(GbfHarness *))
{
    for (uint32_t i = 0; i < max_frames; i++) {
        GB_run_frame(h->gb);
        if (pred(h)) return 1;
    }
    return 0;
}

int harness_run_with_watchdog(GbfHarness *h, uint32_t max_frames,
                              uint32_t max_stall_ms)
{
    /* GBC double-speed: ~8.39 MHz, ~70224 T-cycles per frame (~16.74ms).
       If a frame takes significantly longer than expected, it's a hang. */
    (void)max_stall_ms; /* For now, just use frame count as watchdog */

    uint32_t start_frame = h->frame_count;
    for (uint32_t i = 0; i < max_frames; i++) {
        uint32_t before = h->frame_count;
        GB_run_frame(h->gb);
        /* GB_run_frame should advance exactly one frame.
           If frame_count didn't advance, something is wrong. */
        if (h->frame_count == before) {
            /* vblank didn't fire — possible infinite loop */
            fprintf(stderr, "harness: watchdog — no vblank after frame %u\n",
                    h->frame_count - start_frame);
            return 1;
        }
    }
    return 0;
}

/* ── Memory access ─────────────────────────────────── */

uint8_t harness_read_byte(GbfHarness *h, uint16_t addr)
{
    return GB_safe_read_memory(h->gb, addr);
}

uint16_t harness_read_word(GbfHarness *h, uint16_t addr)
{
    /* Z80/SM83 is little-endian */
    uint8_t lo = GB_safe_read_memory(h->gb, addr);
    uint8_t hi = GB_safe_read_memory(h->gb, addr + 1);
    return ((uint16_t)hi << 8) | lo;
}

void harness_read_block(GbfHarness *h, uint16_t addr,
                        uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        buf[i] = GB_safe_read_memory(h->gb, addr + (uint16_t)i);
    }
}

void harness_read_board(GbfHarness *h, uint8_t board[8][8])
{
#ifdef ADDR_BOARD
    harness_read_block(h, ADDR_BOARD, (uint8_t *)board, 64);
#else
    (void)h;
    memset(board, 0, 64);
    fprintf(stderr, "harness: ADDR_BOARD not defined — run gen_symbols.py\n");
#endif
}

/* ── Game state snapshot ───────────────────────────── */

void harness_snapshot(GbfHarness *h, GameState *s)
{
    memset(s, 0, sizeof(*s));
    harness_read_board(h, s->board);

#ifdef ADDR_CURSOR_X
    s->cursor_x = harness_read_byte(h, ADDR_CURSOR_X);
#endif
#ifdef ADDR_CURSOR_Y
    s->cursor_y = harness_read_byte(h, ADDR_CURSOR_Y);
#endif
#ifdef ADDR_TILE_SELECTED
    s->tile_selected = harness_read_byte(h, ADDR_TILE_SELECTED);
#endif
#ifdef ADDR_PROCESSING_MATCHES
    s->processing_matches = harness_read_byte(h, ADDR_PROCESSING_MATCHES);
#endif
#ifdef ADDR_SCORE
    s->score = harness_read_word(h, ADDR_SCORE);
#endif
#ifdef ADDR_MOVES_LEFT
    s->moves_left = harness_read_byte(h, ADDR_MOVES_LEFT);
#endif
}

/* Two-character tile labels for board dumps. The runtime's board
 * values are opaque indices; these are the example game's kinds. */
static const char *tile_names[] = {
    "  ", "Ru", "Em", "To", "On", "k5", "k6", "k7",
    "k8", "k9", "kA", "kB", "??", "??", "??", "??"
};

void harness_print_board(const uint8_t board[8][8])
{
    printf("  0  1  2  3  4  5  6  7\n");
    for (int y = 0; y < 8; y++) {
        printf("%d ", y);
        for (int x = 0; x < 8; x++) {
            uint8_t t = board[y][x];
            if (t < 16) {
                printf("%s ", tile_names[t]);
            } else {
                printf("%02x ", t);
            }
        }
        printf("\n");
    }
}

/* ── Profiling ─────────────────────────────────────── */

void harness_enable_profiling(GbfHarness *h)
{
    h->profiling_enabled = 1;
    h->overbudget_count = 0;
    h->total_cycles = 0;
}

uint32_t harness_get_overbudget_count(GbfHarness *h)
{
    return h->overbudget_count;
}
