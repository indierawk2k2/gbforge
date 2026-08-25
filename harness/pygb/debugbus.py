"""Debug mailbox client — drives the DEBUG_BUILD ROM's debug hooks.

Mirrors runtime/debug.h. Works over either transport; requires a
ROM built with `make debug` and that ROM's own symbols (symload).

    from pygb import GB, symload
    from pygb.debugbus import DebugBus

    rom = "examples/cascadia/cascadia-debug.gbc"
    gb = GB.launch_headless(rom=rom)
    bus = DebugBus(gb, symload.for_rom(rom))
    bus.set_rng(0x1234)
    bus.enter_mode(DebugBus.MODE_ENDLESS)
    bus.load_board(rows)
    bus.trigger_swap(3, 4, DebugBus.RIGHT)
"""

# Command codes — must match runtime/debug.h
DBG_NONE = 0
DBG_ENTER_MODE = 1
DBG_TRIGGER_SWAP = 2
DBG_REDRAW = 3


class DebugBus:
    """Modes are the title screen's return codes (runtime/debug.h).
    The example game exposes one; a richer game exposes more."""
    MODE_CLASSIC = 0

    UP, DOWN, LEFT, RIGHT = 0, 1, 2, 3

    def __init__(self, gb, syms):
        self.gb = gb
        self.syms = syms
        missing = [n for n in ("debug_req", "debug_arg0", "debug_rng_force")
                   if n not in syms.ram]
        if missing:
            raise RuntimeError(
                f"ROM has no debug mailbox symbols {missing} — "
                f"build with `make -C examples/cascadia debug` "
                f"(symbols from {syms.noi_path})")

    def _post(self, req, arg0=0, arg1=0, arg2=0):
        """Write args then the request byte (request last — the game
        treats a nonzero debug_req as ready-to-consume)."""
        ram = self.syms.ram
        self.gb.write(ram["debug_arg0"], arg0)
        self.gb.write(ram["debug_arg1"], arg1)
        self.gb.write(ram["debug_arg2"], arg2)
        self.gb.write(ram["debug_req"], req)

    def _wait_consumed(self, timeout_frames=600):
        addr = self.syms.ram["debug_req"]
        self.gb.wait_until(lambda gb: gb.read(addr) == DBG_NONE,
                           timeout_frames=timeout_frames)

    def enter_mode(self, mode, arg1=0, arg2=0, settle_frames=300):
        """Jump into a mode from the title screen. The title loop must
        be running (boot + ~600 frames). Waits for consumption plus
        settle time for the mode's init/refill animations."""
        self._post(DBG_ENTER_MODE, mode, arg1, arg2)
        self._wait_consumed()
        self.gb.run_frames(settle_frames)

    def trigger_swap(self, x, y, direction, settle_frames=0):
        """Teleport the cursor to (x,y) and swap toward `direction`.
        Consumed by input_update in whatever mode is active. With
        settle_frames=0 returns as soon as the request is picked up
        (the blocking swap/cascade runs to completion by itself —
        wait on processing_matches or run frames as needed)."""
        self._post(DBG_TRIGGER_SWAP, x, y, direction)
        self._wait_consumed(timeout_frames=1200)
        if settle_frames:
            self.gb.run_frames(settle_frames)

    def redraw(self):
        """Full board redraw after load_board."""
        self._post(DBG_REDRAW)
        self._wait_consumed()

    def set_rng(self, seed):
        """Force the next seed_rng() (board_init) to use `seed`.
        Sticky until cleared with clear_rng()."""
        ram = self.syms.ram
        self.gb.write(ram["debug_rng_seed"], seed & 0xFF)
        self.gb.write(ram["debug_rng_seed"] + 1, (seed >> 8) & 0xFF)
        self.gb.write(ram["debug_rng_force"], 1)

    def clear_rng(self):
        self.gb.write(self.syms.ram["debug_rng_force"], 0)

    def load_board(self, rows):
        """Overwrite board[][] with 8 rows of 8 tile bytes and redraw."""
        flat = [t for row in rows for t in row]
        assert len(flat) == 64
        self.gb.write_block(self.syms.ram["board"], bytes(flat))
        self.redraw()

    def read_board(self):
        flat = self.gb.read_block(self.syms.ram["board"], 64)
        return [flat[r * 8:(r + 1) * 8] for r in range(8)]
