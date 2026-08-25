"""Shared fixtures for the scenario suite.

One gbctl process serves a whole session: launching costs a process
spawn and a ROM load, and every scenario starts by loading a
checkpoint state anyway, so per-test isolation comes from the state
rather than from a fresh emulator.
"""

import os
import sys

import pytest

_HARNESS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, _HARNESS)

from pygb import GB, checkpoints, symload          # noqa: E402
from pygb.debugbus import DebugBus                 # noqa: E402


@pytest.fixture(scope="session")
def debug_rom():
    return checkpoints.ROMS["debug"]


@pytest.fixture(scope="session")
def _gb_session(debug_rom):
    gb = GB.launch_headless(rom=debug_rom)
    yield gb
    gb.close()


@pytest.fixture
def gb(_gb_session, debug_rom):
    """A GB positioned nowhere in particular — load a checkpoint or
    reset the ROM yourself."""
    return _gb_session


@pytest.fixture
def syms(debug_rom):
    return symload.for_rom(debug_rom)


@pytest.fixture
def bus(gb, syms):
    return DebugBus(gb, syms)


@pytest.fixture
def at_title(gb):
    gb.load_state(checkpoints.ensure("title"))
    gb.run_frames(2)
    return gb


@pytest.fixture
def in_game(gb):
    """Classic mode, settled board, fixed seed."""
    gb.load_state(checkpoints.ensure("classic_open"))
    gb.run_frames(2)
    return gb
