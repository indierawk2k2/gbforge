"""Cursor navigation and swap helpers (port of harness_input.c's
high-level layer). Symbol-driven — works on any transport."""


def navigate_to(gb, x, y, max_taps=64):
    """D-pad the cursor to (x, y), reading position from game
    memory. Bounded: a cursor that never moves (wrong screen, a
    swallowed mode entry) raises instead of hanging the suite."""
    for _ in range(max_taps):
        cx = gb.read_sym("cursor_x")
        cy = gb.read_sym("cursor_y")
        if cx == x and cy == y:
            return
        if cx != x:
            gb.tap("RIGHT" if cx < x else "LEFT", 4)
        else:
            gb.tap("DOWN" if cy < y else "UP", 4)
        gb.run_frames(4)
    raise AssertionError(
        f"navigate_to({x},{y}): cursor stuck at ({cx},{cy}) — "
        f"is the game on the expected screen?")


def select_and_swap(gb, x1, y1, x2, y2):
    """Navigate to (x1,y1), grab, and swap toward the adjacent (x2,y2)."""
    navigate_to(gb, x1, y1)
    gb.run_frames(6)
    gb.tap("A", 4)
    gb.run_frames(6)
    if x2 > x1:
        gb.tap("RIGHT", 4)
    elif x2 < x1:
        gb.tap("LEFT", 4)
    elif y2 > y1:
        gb.tap("DOWN", 4)
    else:
        gb.tap("UP", 4)


def wait_cascade(gb, max_frames=600):
    """Run until processing_matches clears. True if it cleared."""
    addr = gb.addr("processing_matches")
    for _ in range(max_frames // 4):
        if gb.read(addr) == 0:
            return True
        gb.run_frames(4)
    return gb.read(addr) == 0
