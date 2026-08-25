"""Game / Mode definitions — the top of the thick SDK.

A game definition instantiates these classes; codegen walks them and
emits C tables the thin runtime drives. Mode differences that legacy
the original hand-written game encoded as copy-pasted choreography and scattered boolean
globals become explicit per-mode data here.
"""

from dataclasses import dataclass, field


@dataclass
class Mode:
    """One game mode's flow configuration.

    refill:        empty cells refill after gravity (endless-style)
                   or stay empty (puzzle-style)
    shake_run:     screen-shake when a match run reaches this length
                   (0 = never) — legacy endless used 5
    shake_passes:  screen-shake when a cascade reaches this many
                   passes (0 = never) — legacy endless used 2
    """
    name: str
    refill: bool = True
    shake_run: int = 0
    shake_passes: int = 0


@dataclass
class Game:
    name: str
    modes: list = field(default_factory=list)
    overlays: list = field(default_factory=list)
    animations: object = None  # BoardAnimations (model.anim)
    title: object = None       # TitleScreen (model.ui)
    abilities: list = field(default_factory=list)  # Ability (model.combat)
    quest: object = None       # QuestGraph (model.quest)
    audio: object = None       # AudioTheme (model.audio)
    scoring: object = None     # Scoring (model.scoring); None = defaults
    ui_strings: dict = None    # name -> text, baked by gen_ui
    ui_counters: dict = None   # name -> (label, n_digits), baked

    def mode_index(self, name):
        for i, m in enumerate(self.modes):
            if m.name == name:
                return i
        raise KeyError(name)
