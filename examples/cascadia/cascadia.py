"""Cascadia — a complete match-3, declared as data.

Everything gameplay-shaped lives here: the mode's refill/shake
policy, every animation timing the runtime interpreter plays, the
result overlays, and the title screen. `python3 -m gbforge build`
turns this into the C configuration tables in generated/; the thin
per-game loop in main_cascadia.c wires them to the shared runtime.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", ".."))

from gbforge.model.anim import (BoardAnimations, CascadeTiming,  # noqa: E402
                                GravityAnim, SwapAnim)
from gbforge.model.game import Game, Mode  # noqa: E402
from gbforge.model.ui import Menu, Overlay, TitleScreen  # noqa: E402

GAME = Game(
    name="cascadia",
    modes=[
        # refill from the top; shake on a 4-run or a 2-deep cascade
        Mode("classic", refill=True, shake_run=4, shake_passes=2),
    ],
    overlays=[
        Overlay("win", pos=(3, 6), size=(14, 5),
                lines=[(1, "YOU WIN"), (2, "1000 POINTS")]),
        Overlay("lose", pos=(3, 6), size=(14, 5),
                lines=[(1, "GAME OVER"), (2, "OUT OF MOVES")]),
    ],
    animations=BoardAnimations(
        swap=SwapAnim(curve=(2, 4, 7, 10, 13, 16)),
        gravity=GravityAnim(delays=(3, 2, 2, 1, 1, 1, 1, 1)),
        timing=CascadeTiming(clear_hold=6, pass_gap=5),
    ),
    title=TitleScreen(
        menu=Menu("mode_select", pos=(4, 10), label_x=6,
                  items=[("PLAY", 0)]),
        logo_text="CASCADIA",
        deco_palettes=(),                     # no deco bar
    ),
)
