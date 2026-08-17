/* gbforge runtime — silent audio glue for games without an
 * AudioTheme. The runtime raises events unconditionally (anim, flow,
 * title); a game that declares no audio links this stub instead of
 * sound_glue.c + generated tables and every event no-ops. */

#include "sound_glue.h"

void ngau_init(void) {}
void ngau_event(uint8_t ev) { (void)ev; }
void ngau_cast(uint8_t sfx_id) { (void)sfx_id; }
void ngau_scene(uint8_t scene) { (void)scene; }
