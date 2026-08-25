"""The cursor bracket must move continuously, never teleport.

The bracket rides the grabbed tile to its destination. That ride is
authored by the animation (`rtc_ride` pins the bracket to the swap's
own easing curve) rather than by the main loop's fixed-speed glide,
because the two ran at different rates and visibly desynced.

But an animation only rides if something DRAWS during it. The swap and
cascade block the main loop, so the runtime calls a frame hook; if the
game never installs one, `rtc_ride` updates a position nobody renders
and the bracket sits at the origin for the whole cascade, then snaps to
the destination the instant the loop resumes.

That is invisible to every state assertion — `cursor_x` is correct the
whole time — so it has to be measured off OAM, frame by frame.
"""

from pygb.nav import navigate_to

from test_gameplay import board_with_pending_match

CURSOR_SLOT = 0
# The swap curve's largest single step is 3px (2,4,7,10,13,16), and
# the free glide moves 6px/frame. Anything above that is a teleport.
MAX_STEP_PX = 8


def _cursor_x(gb):
    return gb.oam(CURSOR_SLOT)[1]


def _grab_and_swap_left(gb, bus, syms):
    """Set up the one scoring swap, then perform it as a player would:
    walk the cursor there, press A, press LEFT."""
    bus.load_board(board_with_pending_match())
    gb.run_frames(10)
    navigate_to(gb, 4, 7)
    gb.run_frames(8)
    gb.tap("A", 4)
    gb.run_frames(8)


def _trace_swap(gb, frames=110):
    """OAM x of the cursor for each frame across a LEFT swap."""
    gb.press("LEFT")
    xs = []
    for i in range(frames):
        gb.run_frames(1)
        if i == 2:
            gb.release("LEFT")
        xs.append(_cursor_x(gb))
    return xs


def test_cursor_never_teleports_across_a_swap(in_game, bus, syms):
    _grab_and_swap_left(in_game, bus, syms)
    xs = _trace_swap(in_game)

    jumps = [(i, xs[i - 1], xs[i])
             for i in range(1, len(xs))
             if abs(xs[i] - xs[i - 1]) > MAX_STEP_PX]
    assert not jumps, (
        f"cursor teleported {jumps} — it should be carried by the swap "
        f"animation, not snapped when the main loop resumes")


def test_cursor_is_drawn_during_the_swap_slide(in_game, bus, syms):
    """The bracket must occupy intermediate positions, not just the
    two endpoints. A run that only ever shows origin and destination
    means nothing rendered while the animation played."""
    _grab_and_swap_left(in_game, bus, syms)
    xs = _trace_swap(in_game)

    positions = sorted(set(xs))
    assert len(positions) > 2, (
        f"cursor only ever appeared at {positions} — the swap slide "
        f"drew no intermediate frames")


def test_cursor_ends_on_the_destination_tile(in_game, bus, syms):
    """Whatever the path, it has to land where the model says."""
    _grab_and_swap_left(in_game, bus, syms)
    _trace_swap(in_game)
    in_game.run_frames(30)

    gx = in_game.read(syms.ram["cursor_x"])
    assert gx == 3, f"model cursor is at x={gx}, expected the destination 3"
    # rt_screen_off_x=3, sprite x = (gx<<4) + 3 - 3 + 8
    assert _cursor_x(in_game) == (gx << 4) + 8, (
        "cursor sprite is not on the tile the model says it is")
