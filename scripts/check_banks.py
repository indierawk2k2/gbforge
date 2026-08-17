#!/usr/bin/env python3
"""Fail the build if any linked area or symbol falls outside its ROM
bank window. The linker is silent about this: during M10 a full bank 1
placed rt_ai_best_swap at 0x837C (a VRAM address) and callers executed
garbage. This check makes that class of fault a build error.

Usage: check_banks.py <rom.noi> [rom2.noi ...]

Rules (addresses in .noi encode bank<<16 | addr):
  - ROM0 areas   (base < 0x4000, bank 0): base + len <= 0x4000
  - banked areas (0x4000 <= base < 0x8000): base + len <= 0x8000
  - RAM areas    (base >= 0x8000): ignored
  - per-symbol backup: any bank>0 symbol below 0xA000 must sit inside
    0x4000-0x7FFF (GBDK's _r* hardware-register EQUs are exempt)
"""

import re
import sys


def check(path):
    s_areas, l_areas = {}, {}
    symbols = []
    for line in open(path):
        parts = line.split()
        if len(parts) != 3 or parts[0] != "DEF":
            continue
        name, val = parts[1], int(parts[2], 16)
        if name.startswith("s__"):
            s_areas[name[3:]] = val
        elif name.startswith("l__"):
            l_areas[name[3:]] = val
        else:
            symbols.append((name, val))

    errors = []

    for area, start in s_areas.items():
        length = l_areas.get(area)
        if not length:
            continue
        bank, base = start >> 16, start & 0xFFFF
        end = base + length
        if base >= 0x8000:
            continue  # RAM
        if bank == 0:
            # ROM0 area: must start AND end below 0x4000 — a base
            # past 0x4000 means it already spilled out of HOME
            if end > 0x4000 or base >= 0x4000:
                errors.append(
                    f"area {area}: ROM0 overflow — "
                    f"{hex(base)}+{hex(length)} ends at {hex(end)} > 0x4000")
        else:
            if end > 0x8000:
                errors.append(
                    f"area {area} (bank {bank}): overflow — "
                    f"{hex(base)}+{hex(length)} ends at {hex(end)} > 0x8000")

    for name, val in symbols:
        bank, addr = val >> 16, val & 0xFFFF
        if bank == 0 or addr >= 0xA000:
            continue
        if re.match(r"^_r[A-Z]", name):
            continue  # hardware-register EQU constants
        if not (0x4000 <= addr < 0x8000):
            errors.append(
                f"symbol {name}: bank {bank} address {hex(addr)} is "
                f"outside the 0x4000-0x7FFF window")

    return errors


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    failed = False
    for path in sys.argv[1:]:
        errors = check(path)
        if errors:
            failed = True
            print(f"check_banks: {path}: FAIL")
            for e in errors:
                print(f"  {e}")
        else:
            print(f"check_banks: {path}: ok")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
