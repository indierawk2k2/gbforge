"""Pixel-level assertions.

Memory tests prove the model is right; these prove the screen agrees
with it. The two halves catch opposite bugs — a revert that repaints
but forgets the board, and a revert that fixes the board but leaves
the old tiles on screen — and neither half can see the other's.
"""

import os

from pygb import checkpoints
from pygb.screen import Frame, Recording, expect_screen

HARNESS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The 8x8 board occupies bkg tiles 1..16 on both axes, at SCX/SCY = 5.
BOARD_RECT = (3, 3, 131, 131)
# The sidebar counters live in the window layer on the right.
SIDEBAR_RECT = (142, 0, 160, 40)


def _golden(name):
    return os.path.join(HARNESS, "golden", f"{name}.png")


def test_board_renders_something(in_game):
    """The board area is not a flat field — a blit that silently did
    nothing leaves the screen one colour, which every other visual
    assertion would happily pass."""
    f = Frame.grab(in_game)
    colors = {f.pixel(x, y)
              for y in range(*BOARD_RECT[1::2])
              for x in range(BOARD_RECT[0], BOARD_RECT[2], 4)}
    assert len(colors) >= 4, f"board area has only {len(colors)} colours"


def test_title_matches_golden(at_title):
    expect_screen(at_title, "title", artifacts_dir=os.path.join(
        HARNESS, "output", "failures"))


def test_classic_open_matches_golden(in_game):
    expect_screen(in_game, "classic_open", artifacts_dir=os.path.join(
        HARNESS, "output", "failures"))


def test_reverted_swap_leaves_no_pixel_trace(in_game, bus):
    """A swap that doesn't match animates out and back. When it
    settles, the screen must be pixel-identical to before it started —
    a stale tile left at the destination is invisible to any memory
    assertion."""
    from test_gameplay import board_with_pending_match

    bus.load_board(board_with_pending_match())
    in_game.run_frames(20)
    before = Frame.grab(in_game)

    bus.trigger_swap(0, 0, bus.RIGHT)      # checkerboard corner: no match
    in_game.run_frames(90)

    after = Frame.grab(in_game)
    diff = after.diff_count(before, BOARD_RECT, tolerance=0)
    assert diff == 0, (
        f"{diff} board pixels changed across a reverted swap")


def test_board_is_stable_when_idle(in_game):
    """Nothing on the board may flicker while no input arrives.

    Recorded as a hash sequence rather than a start/end comparison:
    a one-frame tear that repairs itself is exactly the artifact a
    two-sample check cannot see."""
    rec = Recording(in_game, region=BOARD_RECT).capture(30)
    rec.expect_region_stable()


def test_lose_overlay_draws_baked_text(in_game, bus, syms):
    """Driving the game to its GAME OVER card exercises the whole
    text pipeline: gen_ui baked the two lines into tiles at build
    time, and the runtime only uploads them. If the pool base or the
    span geometry is wrong, this is where it shows."""
    from test_gameplay import board_with_pending_match

    bus.load_board(board_with_pending_match())
    in_game.write(syms.ram["moves_left"], 1)

    bus.trigger_swap(4, 7, bus.LEFT)       # the one scoring swap
    in_game.run_frames(240)

    assert in_game.read(syms.ram["moves_left"]) == 0
    expect_screen(in_game, "lose_overlay", artifacts_dir=os.path.join(
        HARNESS, "output", "failures"))
