"""Booted to the title screen, menu idle.

The cheapest checkpoint: every gameplay recipe starts from here, and
loading a state is ~1ms against ~600 frames of boot animation.
"""

ROM = "debug"


def make(gb, bus):
    gb.run_frames(600)          # boot ROM + logo + title bring-up
