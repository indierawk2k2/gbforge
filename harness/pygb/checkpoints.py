"""Checkpoint recipes and the ROM-hash-keyed savestate cache.

Checkpoints are RECIPES, not committed .state files. A recipe is a
Python file in harness/checkpoints/recipes/ defining:

    ROM = "debug"            # or "retail"
    def make(gb, bus):       # drive the ROM into the target state
        ...

`ensure(name)` returns a .state path from the cache, regenerating it
headlessly (gbctl) when missing. The cache directory is keyed by the
ROM's SHA1, so a rebuilt ROM can never silently load a stale state —
the old cache directory simply stops being used.

CLI:  python3 -m pygb.checkpoints --all      regenerate everything
      python3 -m pygb.checkpoints name...    specific recipes
      python3 -m pygb.checkpoints --list
"""

import hashlib
import importlib.util
import os
import sys

from . import symload
from .client import GB
from .debugbus import DebugBus

_HARNESS = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
_REPO = os.path.dirname(_HARNESS)

RECIPES_DIR = os.path.join(_HARNESS, "checkpoints", "recipes")
CACHE_DIR = os.path.join(_HARNESS, "checkpoints", "cache")

ROMS = {
    "retail": os.path.join(_REPO, "examples", "cascadia", "cascadia.gbc"),
    "debug": os.path.join(_REPO, "examples", "cascadia",
                          "cascadia-debug.gbc"),
}


def rom_sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        h.update(f.read())
    return h.hexdigest()


def _load_recipe(name):
    path = os.path.join(RECIPES_DIR, f"{name}.py")
    if not os.path.exists(path):
        raise FileNotFoundError(f"no recipe {name!r} in {RECIPES_DIR}")
    spec = importlib.util.spec_from_file_location(f"recipe_{name}", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def list_recipes():
    return sorted(f[:-3] for f in os.listdir(RECIPES_DIR)
                  if f.endswith(".py") and not f.startswith("_"))


def rom_for(name):
    """(rom_kind, rom_path) a recipe runs on."""
    mod = _load_recipe(name)
    kind = getattr(mod, "ROM", "debug")
    return kind, ROMS[kind]


def state_path(name, rom_path):
    return os.path.join(CACHE_DIR, rom_sha1(rom_path)[:12], f"{name}.state")


def generate(name, gb=None):
    """Run a recipe headlessly and cache the resulting savestate.
    Pass an existing GB on the right ROM to batch several recipes."""
    mod = _load_recipe(name)
    kind = getattr(mod, "ROM", "debug")
    rom = ROMS[kind]
    path = state_path(name, rom)
    os.makedirs(os.path.dirname(path), exist_ok=True)

    own = gb is None
    if own:
        gb = GB.launch_headless(rom=rom)
    try:
        gb.load_rom(rom)  # fresh boot for every recipe
        # Any DEBUG-built ROM (legacy or ng) carries the mailbox;
        # retail ROMs don't — those recipes get bus=None.
        try:
            bus = DebugBus(gb, symload.for_rom(rom))
        except RuntimeError:
            bus = None
        mod.make(gb, bus)
        gb.save_state(path)
    finally:
        if own:
            gb.close()
    return path


def ensure(name):
    """Cached state path for a recipe, regenerating if missing. The
    hash-keyed directory makes stale loads structurally impossible."""
    _, rom = rom_for(name)
    path = state_path(name, rom)
    if not os.path.exists(path):
        print(f"checkpoints: generating {name} → {path}")
        generate(name)
    return path


def main(argv):
    if "--list" in argv:
        for n in list_recipes():
            print(n)
        return 0
    names = list_recipes() if ("--all" in argv or not argv) else argv
    # Group by ROM so one gbctl serves each ROM's recipes
    by_rom = {}
    for n in names:
        kind, rom = rom_for(n)
        by_rom.setdefault(rom, []).append(n)
    for rom, group in by_rom.items():
        gb = GB.launch_headless(rom=rom)
        try:
            for n in group:
                path = generate(n, gb=gb)
                print(f"checkpoints: {n} → {os.path.relpath(path, _HARNESS)}")
        finally:
            gb.close()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
