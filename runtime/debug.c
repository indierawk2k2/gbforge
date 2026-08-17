/* Debug mailbox storage — see debug.h. Empty in retail builds. */

#include "debug.h"

#ifdef DEBUG_BUILD

volatile uint8_t debug_req = DBG_NONE;
volatile uint8_t debug_arg0 = 0;
volatile uint8_t debug_arg1 = 0;
volatile uint8_t debug_arg2 = 0;

volatile uint8_t debug_phase = 0;
volatile uint8_t debug_rng_force = 0;
volatile uint16_t debug_rng_seed = 0;

#endif /* DEBUG_BUILD */
