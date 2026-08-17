"""gbforge CLI.

    python3 -m gbforge build <game_dir> -o <out_dir>

Loads <game_dir>/game.py (which must define GAME) and runs every
generator against it, emitting C into <out_dir>.
"""

import importlib.util
import os
import sys

from .codegen import (gen_abilities, gen_anim, gen_audio, gen_modes,
                      gen_quest, gen_title, gen_ui)


def _spec_path(game_dir):
    """game.py, or <dirname>.py (examples name the spec after
    the game: examples/cascadia/cascadia.py)."""
    default = os.path.join(game_dir, "game.py")
    if os.path.exists(default):
        return default
    named = os.path.join(
        game_dir, os.path.basename(os.path.normpath(game_dir)) + ".py")
    if os.path.exists(named):
        return named
    return default


def _load_game(game_dir):
    path = _spec_path(game_dir)
    spec = importlib.util.spec_from_file_location("game_def", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.GAME


def main(argv):
    if len(argv) < 3 or argv[0] != "build" or "-o" not in argv:
        print(__doc__)
        return 2
    game_dir = argv[1]
    out_dir = argv[argv.index("-o") + 1]
    game = _load_game(game_dir)
    emitted = []
    emitted += gen_modes.emit(game, out_dir)
    emitted += gen_ui.emit(game, out_dir)
    if game.animations is not None:
        emitted += gen_anim.emit(game, out_dir)
    emitted += gen_title.emit(game, out_dir)
    emitted += gen_abilities.emit(game, out_dir)
    emitted += gen_quest.emit(game, out_dir)
    emitted += gen_audio.emit(game, out_dir)
    print(f"gbforge: {game.name} -> {out_dir}: {', '.join(emitted)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
