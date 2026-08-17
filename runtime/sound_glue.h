#ifndef GBFORGE_SOUND_GLUE_H_INCLUDED
#define GBFORGE_SOUND_GLUE_H_INCLUDED

/* gbforge runtime — audio glue over the game's sound driver.
 *
 * HOME-resident so every bank context (board flow, title, quest,
 * store) can raise events without switch overhead. The generated
 * tables (gen/audio_config.c) map events to effect ids and scenes
 * to music tracks; the legacy driver (sound.c, VBL-pumped) does the
 * actual register work.
 */

#include <stdint.h>

/* The fixed runtime event vocabulary (mirrors
 * gbforge.model.audio.EVENTS — append-only; the generated
 * audio_config.c cross-checks the count). Games without an
 * AudioTheme link sound_glue_stub.c and every event no-ops. */
#define NGAU_NONE 0xFF
#define NGAU_EV_CURSOR_MOVE 0
#define NGAU_EV_TILE_SELECT 1
#define NGAU_EV_TILE_DESELECT 2
#define NGAU_EV_SWAP 3
#define NGAU_EV_SWAP_FAIL 4
#define NGAU_EV_MATCH_3_FIRE 5
#define NGAU_EV_MATCH_3_WATER 6
#define NGAU_EV_MATCH_3_EARTH 7
#define NGAU_EV_MATCH_4_FIRE 8
#define NGAU_EV_MATCH_4_WATER 9
#define NGAU_EV_MATCH_4_EARTH 10
#define NGAU_EV_MATCH_5 11
#define NGAU_EV_METAL_MATCH 12
#define NGAU_EV_CHAIN_2 13
#define NGAU_EV_CHAIN_3 14
#define NGAU_EV_CHAIN_4 15
#define NGAU_EV_CHAIN_5 16
#define NGAU_EV_TRANSMUTE_BRONZE 17
#define NGAU_EV_TRANSMUTE_SILVER 18
#define NGAU_EV_TRANSMUTE_GOLD 19
#define NGAU_EV_TRANSMUTE_PLATINUM 20
#define NGAU_EV_TRANSMUTE_EMERALD 21
#define NGAU_EV_TRANSMUTE_RUBY 22
#define NGAU_EV_TRANSMUTE_OBSIDIAN 23
#define NGAU_EV_TRANSMUTE_AETHER 24
#define NGAU_EV_FALL 25
#define NGAU_EV_REFILL 26
#define NGAU_EV_CAST 27
#define NGAU_EV_SPELL_CYCLE 28
#define NGAU_EV_SPELL_PURCHASE 29
#define NGAU_EV_MENU_MOVE 30
#define NGAU_EV_MENU_CONFIRM 31
#define NGAU_EV_MENU_CANCEL 32
#define NGAU_EV_STORE_OPEN 33
#define NGAU_EV_STORE_CLOSE 34
#define NGAU_EV_WIN 35
#define NGAU_EV_LOSE 36
#define NGAU_EV_WARNING 37
#define NGAU_EVENT_COUNT 38

/* sound_init + saved volume restore. Call once at boot. */
void ngau_init(void);

/* Raise a theme event (NGAU_EV_*). Silent if unmapped or muted. */
void ngau_event(uint8_t ev);

/* Play an effect by direct id (per-ability cast_sfx); 0xFF falls
 * back to the generic cast event. */
void ngau_cast(uint8_t sfx_id);

/* Switch background music for a scene (NGAU_SCENE_*). A scene with
 * no track keeps the current music playing. */
void ngau_scene(uint8_t scene);

#endif
