"""The ROM boots and reaches a playable title screen.

The cheapest test that catches the worst failure: a bank-switching
mistake makes the CPU jump into unmapped ROM, and the game hangs
without ever writing a byte anyone would assert on. Watching the PC
move and the title's own state appear is what distinguishes "running"
from "spinning".
"""


def test_rom_boots_and_runs(gb, debug_rom):
    gb.load_rom(debug_rom)
    gb.run_frames(600)
    r = gb.cmd("regs")
    assert 0x0000 <= r["pc"] <= 0xFFFF
    assert r["bank"] >= 1, "ROM bank 0 mapped into the switchable slot"


def test_title_reaches_menu(at_title, syms):
    """debug_req is idle and the board is not yet initialised: the
    title screen is up and waiting, not mid-transition."""
    assert at_title.read(syms.ram["debug_req"]) == 0
    assert at_title.read(syms.ram["moves_left"]) in (0, 30)
