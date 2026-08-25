#!/usr/bin/env python3
"""Regenerate harness/golden/*.png from the current ROM.

Goldens are outputs, so they are regenerated rather than hand-edited —
but never automatically: run this only after LOOKING at the new
frames and agreeing that the change is the one you meant to make. A
golden updated reflexively turns a regression suite into a recorder.

    python3 scripts/update_golden.py [name ...]
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from pygb import GB, checkpoints                      # noqa: E402
from pygb.screen import Frame                         # noqa: E402
from pygb.debugbus import DebugBus                    # noqa: E402
from pygb import symload                              # noqa: E402

HARNESS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(HARNESS, "golden")


def shot_title(gb, bus, syms):
    gb.load_state(checkpoints.ensure("title"))
    gb.run_frames(2)


def shot_classic_open(gb, bus, syms):
    gb.load_state(checkpoints.ensure("classic_open"))
    gb.run_frames(2)


def shot_lose_overlay(gb, bus, syms):
    sys.path.insert(0, os.path.join(HARNESS, "scenarios"))
    from test_gameplay import board_with_pending_match
    gb.load_state(checkpoints.ensure("classic_open"))
    gb.run_frames(2)
    bus.load_board(board_with_pending_match())
    gb.write(syms.ram["moves_left"], 1)
    bus.trigger_swap(4, 7, bus.LEFT)
    gb.run_frames(240)


SHOTS = {
    "title": shot_title,
    "classic_open": shot_classic_open,
    "lose_overlay": shot_lose_overlay,
}


def main(argv):
    names = argv or sorted(SHOTS)
    rom = checkpoints.ROMS["debug"]
    gb = GB.launch_headless(rom=rom)
    syms = symload.for_rom(rom)
    bus = DebugBus(gb, syms)
    os.makedirs(GOLDEN, exist_ok=True)
    try:
        for name in names:
            SHOTS[name](gb, bus, syms)
            path = Frame.grab(gb).save_png(os.path.join(GOLDEN, f"{name}.png"))
            print(f"golden: {name} -> {os.path.relpath(path, HARNESS)}")
    finally:
        gb.close()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
