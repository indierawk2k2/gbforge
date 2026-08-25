"""Title screen layout, measured off the frame.

Centring is the kind of thing that looks fine to whoever placed it and
wrong to everyone else, so it is asserted numerically rather than
eyeballed: find the ink, compare the margins.

The old logo was a monospaced strip of 16x16 cells placed at a
hand-chosen tile, which put it 2 tiles off centre and gave every
narrow letter a wide gap. Both are now the generator's job — it bakes
the text in the display face and centres the INK at pixel precision —
so both are checkable.
"""

from pygb import checkpoints
from pygb.screen import Frame

LOGO_BAND = (24, 56)      # scanlines the logo occupies
MENU_BAND = (76, 92)      # the arrow + first label
INK = 500                 # sum(rgb) below this counts as a mark


def _ink_extent(frame, band):
    y0, y1 = band
    xs = [x for y in range(y0, y1) for x in range(frame.w)
          if sum(frame.pixel(x, y)) < INK]
    assert xs, f"no ink at all in scanlines {band}"
    return min(xs), max(xs)


def _assert_centered(frame, band, what, tolerance=1):
    left, right = _ink_extent(frame, band)
    right_margin = (frame.w - 1) - right
    assert abs(left - right_margin) <= tolerance, (
        f"{what} is off centre: ink spans {left}..{right}, "
        f"margins {left} left vs {right_margin} right")
    return left, right


def test_logo_is_centered(at_title):
    _assert_centered(Frame.grab(at_title), LOGO_BAND, "the logo")


def test_menu_block_is_centered(at_title):
    """The arrow and the label are one block. The arrow can only sit
    on an 8px tile boundary, so the generator snaps it and lets the
    GAP absorb the error — if it let the label follow the snapped
    arrow instead, the block would sit up to 4px off centre."""
    _assert_centered(Frame.grab(at_title), MENU_BAND, "the menu block")


def test_logo_uses_the_display_face(at_title):
    """The logo must not be the 8x8 UI font scaled up.

    A doubled bitmap has no odd-width runs: every horizontal stroke is
    an even number of pixels and starts on an even column. The 16px
    face is drawn at a 3px stem, so odd runs are everywhere.
    """
    frame = Frame.grab(at_title)
    y0, y1 = LOGO_BAND
    odd_runs = 0
    for y in range(y0, y1):
        run = 0
        for x in range(frame.w):
            if sum(frame.pixel(x, y)) < INK:
                run += 1
            else:
                if run and run % 2:
                    odd_runs += 1
                run = 0
    assert odd_runs > 8, (
        f"only {odd_runs} odd-width strokes in the logo — this looks "
        f"like a pixel-doubled 8x8 font, not the 16px display face")


# Proportional spacing is NOT asserted here. It is real, but it is not
# cleanly visible in a frame: a monospaced strip of narrow and wide
# letters produces varying ink-start pitches too, so the obvious pixel
# test passes on the very layout it was meant to reject. The property
# is checked exactly where it is exact — against the face's own
# advance table, in tests/test_asset_contracts.py.
