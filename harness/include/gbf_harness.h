#ifndef GBF_HARNESS_H
#define GBF_HARNESS_H

#include <stdint.h>
#include <stddef.h>

/* SameBoy core type — forward declared; full definition from gb.h */
typedef struct GB_gameboy_s GB_gameboy_t;

/* Screen dimensions (Game Boy: 160x144) */
#define GB_SCREEN_W 160
#define GB_SCREEN_H 144
#define GB_PIXEL_COUNT (GB_SCREEN_W * GB_SCREEN_H)

/* ── Harness context ────────────────────────────────── */

typedef struct {
    GB_gameboy_t *gb;
    uint32_t pixels[GB_PIXEL_COUNT];
    uint32_t frame_count;

    /* Profiling */
    int profiling_enabled;
    uint32_t overbudget_count;
    uint64_t total_cycles;

    /* Paths (set during init) */
    const char *rom_path;
    const char *boot_rom_path;
} GbfHarness;

/* ── Lifecycle ──────────────────────────────────────── */

/* Initialize harness: load ROM, boot ROM, configure callbacks.
   Returns 0 on success, non-zero on failure. */
int  harness_init(GbfHarness *h);

/* Initialize with custom paths (overrides compiled-in defaults). */
int  harness_init_with_paths(GbfHarness *h,
                             const char *rom_path,
                             const char *boot_rom_path);

/* Free all resources. */
void harness_free(GbfHarness *h);

/* ── Frame execution ───────────────────────────────── */

/* Run exactly N frames. */
void harness_run_frames(GbfHarness *h, uint32_t n);

/* Run until predicate returns non-zero, or max_frames reached.
   Returns 1 if predicate triggered, 0 if timed out. */
int  harness_run_until(GbfHarness *h, uint32_t max_frames,
                       int (*pred)(GbfHarness *));

/* Run with watchdog — aborts if a single frame exceeds max_stall_ms
   of emulated time. Returns 0 on success, 1 on hang detected. */
int  harness_run_with_watchdog(GbfHarness *h, uint32_t max_frames,
                               uint32_t max_stall_ms);

/* ── Memory access ─────────────────────────────────── */

uint8_t  harness_read_byte(GbfHarness *h, uint16_t addr);
uint16_t harness_read_word(GbfHarness *h, uint16_t addr);
void     harness_read_block(GbfHarness *h, uint16_t addr,
                            uint8_t *buf, size_t len);

/* Read the 8x8 board from game memory (requires symbol addresses). */
void harness_read_board(GbfHarness *h, uint8_t board[8][8]);

/* ── Game state snapshot ───────────────────────────── */

/* The fields every gbforge game exposes, read through the generated
 * ADDR_* defines. A game with more state adds symbols to
 * scripts/gen_symbols.py and reads them with harness_read_byte —
 * this struct stays the common denominator, so a scenario written
 * against it runs against any game built on the runtime. */
typedef struct {
    uint8_t  board[8][8];
    uint8_t  cursor_x, cursor_y;
    uint16_t score;
    uint8_t  moves_left;
    uint8_t  tile_selected;
    uint8_t  processing_matches;
} GameState;

void harness_snapshot(GbfHarness *h, GameState *s);
void harness_print_board(const uint8_t board[8][8]);

/* ── Profiling ─────────────────────────────────────── */

void     harness_enable_profiling(GbfHarness *h);
uint32_t harness_get_overbudget_count(GbfHarness *h);

/* ── Test assertion macros (same style as tests/test.h) ─ */

extern int _harness_test_pass;
extern int _harness_test_fail;
extern int _harness_test_current_failed;
extern const char *_harness_test_current_name;

#define SCENARIO(name) \
    static void scenario_##name(GbfHarness *h); \
    static void run_scenario_##name(GbfHarness *h) { \
        _harness_test_current_name = #name; \
        _harness_test_current_failed = 0; \
        scenario_##name(h); \
        if (!_harness_test_current_failed) { _harness_test_pass++; } \
    } \
    static void scenario_##name(GbfHarness *h)

#define ASSERT(expr) do { \
    if (!(expr)) { \
        printf("  FAIL %s: %s (line %d)\n", \
               _harness_test_current_name, #expr, __LINE__); \
        _harness_test_current_failed = 1; _harness_test_fail++; return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { \
        printf("  FAIL %s: %s == %s (%ld != %ld) (line %d)\n", \
               _harness_test_current_name, #a, #b, _a, _b, __LINE__); \
        _harness_test_current_failed = 1; _harness_test_fail++; return; \
    } \
} while(0)

#define ASSERT_NEQ(a, b) do { \
    long _a = (long)(a), _b = (long)(b); \
    if (_a == _b) { \
        printf("  FAIL %s: %s != %s (both %ld) (line %d)\n", \
               _harness_test_current_name, #a, #b, _a, __LINE__); \
        _harness_test_current_failed = 1; _harness_test_fail++; return; \
    } \
} while(0)

#define ASSERT_MEM_EQ(h, addr, expected) \
    ASSERT_EQ(harness_read_byte(h, addr), expected)

#define ASSERT_BOARD_TILE(h, x, y, tile) \
    ASSERT_MEM_EQ(h, ADDR_BOARD + (y)*8 + (x), tile)

#define ASSERT_GT(a, b) do { \
    long _a = (long)(a), _b = (long)(b); \
    if (!(_a > _b)) { \
        printf("  FAIL %s: %s > %s (%ld <= %ld) (line %d)\n", \
               _harness_test_current_name, #a, #b, _a, _b, __LINE__); \
        _harness_test_current_failed = 1; _harness_test_fail++; return; \
    } \
} while(0)

#define RUN_SCENARIO(name, h) run_scenario_##name(h)

#define SCENARIO_SUMMARY(suite) do { \
    printf("%s: %d passed, %d failed\n", suite, \
           _harness_test_pass, _harness_test_fail); \
    return _harness_test_fail > 0 ? 1 : 0; \
} while(0)

#endif /* GBF_HARNESS_H */
