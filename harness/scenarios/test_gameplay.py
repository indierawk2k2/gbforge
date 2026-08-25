"""Gameplay assertions driven through the debug mailbox.

The pattern every scenario here uses: inject a board, trigger a swap,
wait for the cascade to finish, read memory, assert. No screen
scraping, no scripted button choreography that breaks when a menu
gains a row — the mailbox puts the game in the exact state the
assertion is about.
"""

from pygb.nav import wait_cascade

# The example's kinds are 1..4; 0 is TILE_EMPTY.
RUBY, EMERALD, TOPAZ, ONYX = 1, 2, 3, 4
POINTS_PER_TILE = 10


def board_with_pending_match():
    """A board whose only match appears after swapping (4,7)<->(3,7).

    Row 7 is  TO ON TO RUBY EM RUBY RUBY TO: moving the EMERALD at x=4
    left puts RUBY at x=4,5,6 — a run of three. Every other cell is a
    two-kind checkerboard, which cannot match in any direction, so the
    swap's score is attributable to exactly this run.
    """
    rows = [[EMERALD if (x + y) % 2 else ONYX for x in range(8)]
            for y in range(8)]
    rows[7] = [TOPAZ, ONYX, TOPAZ, RUBY, EMERALD, RUBY, RUBY, TOPAZ]
    rows[6] = [ONYX, TOPAZ, ONYX, TOPAZ, RUBY, TOPAZ, ONYX, TOPAZ]
    return rows


def test_injected_board_lands_in_memory(in_game, bus):
    rows = board_with_pending_match()
    bus.load_board(rows)
    assert bus.read_board() == rows


def test_matching_swap_scores(in_game, bus, syms):
    bus.load_board(board_with_pending_match())
    before = in_game.read16(syms.ram["score"])

    bus.trigger_swap(4, 7, bus.LEFT)
    assert wait_cascade(in_game), "cascade never finished"

    after = in_game.read16(syms.ram["score"])
    gained = after - before
    assert gained == 3 * POINTS_PER_TILE, (
        f"the three-run scored {gained}, expected exactly "
        f"{3 * POINTS_PER_TILE} — a different number means either the "
        f"scoring table moved or the refill dropped a bonus match")


def test_non_matching_swap_reverts(in_game, bus, syms):
    """A swap that makes no match must leave the board byte-identical.

    This is the assertion that catches a revert animation which
    updates the screen but forgets the model (or the reverse): both
    halves are checked, memory here and pixels in test_visual.
    """
    rows = board_with_pending_match()
    bus.load_board(rows)
    score_before = in_game.read16(syms.ram["score"])

    bus.trigger_swap(0, 0, bus.RIGHT)     # checkerboard corner: no match
    in_game.run_frames(60)
    wait_cascade(in_game)

    assert bus.read_board() == rows, "a no-match swap changed the board"
    assert in_game.read16(syms.ram["score"]) == score_before


def test_move_counter_decrements_once_per_swap(in_game, bus, syms):
    bus.load_board(board_with_pending_match())
    before = in_game.read(syms.ram["moves_left"])

    bus.trigger_swap(4, 7, bus.LEFT)
    assert wait_cascade(in_game)

    after = in_game.read(syms.ram["moves_left"])
    assert after == before - 1, (
        f"moves went {before} -> {after}; a cascade must cost exactly "
        f"one move no matter how many passes it runs")


def test_board_never_holds_an_unresolved_match(in_game, bus):
    """After any cascade settles, no three-in-a-row may remain.

    A refill that drops a matching tile and doesn't rescan leaves the
    board in a state the player can see but the engine considers
    finished — the classic 'dead match' bug.
    """
    bus.load_board(board_with_pending_match())
    bus.trigger_swap(4, 7, bus.LEFT)
    assert wait_cascade(in_game)
    in_game.run_frames(30)

    rows = bus.read_board()
    for y in range(8):
        for x in range(6):
            run = rows[y][x:x + 3]
            assert not (run[0] and run[0] == run[1] == run[2]), \
                f"horizontal match left unresolved at ({x},{y}): {run}"
    for x in range(8):
        for y in range(6):
            run = [rows[y + i][x] for i in range(3)]
            assert not (run[0] and run[0] == run[1] == run[2]), \
                f"vertical match left unresolved at ({x},{y}): {run}"


def test_board_holds_no_empty_tiles_when_settled(in_game, bus):
    """Gravity + refill must leave a full board. An empty cell that
    survives the settle is a hole the player can never clear."""
    bus.load_board(board_with_pending_match())
    bus.trigger_swap(4, 7, bus.LEFT)
    assert wait_cascade(in_game)
    in_game.run_frames(30)

    rows = bus.read_board()
    holes = [(x, y) for y in range(8) for x in range(8) if rows[y][x] == 0]
    assert not holes, f"settled board has empty cells at {holes}"
