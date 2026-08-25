"""Classic mode, board settled, on a FIXED seed.

Deterministic by construction: debug_rng_force pins the seed before
board_fill runs, so the 8x8 layout is a property of the ROM rather
than of DIV_REG at the instant someone pressed START. Every scenario
that asserts on tile positions depends on this.
"""

ROM = "debug"
SEED = 0x1234


def make(gb, bus):
    gb.run_frames(600)
    bus.set_rng(SEED)
    bus.enter_mode(bus.MODE_CLASSIC)
