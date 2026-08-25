"""Runtime .noi symbol loading — for ROMs other than the retail build.

The generated pygb.symbols module always reflects the retail ROM.
Debug-build workflows (mailbox, checkpoints) must use the debug ROM's
own addresses — adding the mailbox shifts every variable after it —
so load them at runtime with:

    syms = symload.load_noi("examples/cascadia/cascadia-debug.noi")
    gb.write(syms.ram["debug_req"], 1)

Parsing matches harness/scripts/gen_symbols.py: full-name .noi symbols only,
leading underscore stripped, linker section markers dropped.
"""

import os
import re
from collections import namedtuple

Symbols = namedtuple("Symbols", ["ram", "home", "banked", "noi_path"])

_RAM_RANGES = ((0xC000, 0xE000), (0xFF80, 0xFFFF))


def _is_ram(addr):
    return any(lo <= addr < hi for lo, hi in _RAM_RANGES)


def load_noi(path):
    """Parse a GBDK/SDCC .noi file into RAM / HOME / banked tables."""
    ram, home, banked = {}, {}, {}
    with open(path) as f:
        for line in f:
            m = re.match(r"DEF\s+(\S+)\s+0x([0-9A-Fa-f]+)", line.strip())
            if not m:
                continue
            name, raw = m.group(1), int(m.group(2), 16)
            if not name.startswith("_") or name.startswith("__"):
                continue
            name = name[1:]
            if raw > 0xFFFF:
                banked[name] = (raw >> 16, raw & 0xFFFF)
            elif _is_ram(raw):
                ram[name] = raw
            else:
                home[name] = raw
    return Symbols(ram=ram, home=home, banked=banked,
                   noi_path=os.path.abspath(path))


def for_rom(rom_path):
    """Symbols for the .noi sitting next to a ROM file."""
    return load_noi(os.path.splitext(rom_path)[0] + ".noi")
