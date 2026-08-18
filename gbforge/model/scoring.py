"""Scoring — every match reward number in one declarative place.

The shared runtime engine is the ONLY scorer (every mode resolves
through flow_resolve), so cross-mode consistency is structural; this
model makes the NUMBERS part of the game definition too. gen_scoring
projects it to const C tables the engine indexes, and the reference
sim reads the same dataclass — one source, three consumers.

Shapes (L/T crossings, simultaneous rows) are engine SEMANTICS, not
numbers: a run of N scores as a run of N wherever and however it
lies, and crossing runs score once per run exactly as legacy did.
What varies by design is only what a run is worth — that's what
lives here.
"""

from dataclasses import dataclass


@dataclass
class Scoring:
    # manna awarded for a manna run, indexed by run length (0..8;
    # longer runs clamp to the last entry). Legacy match.c curve:
    # 3 tiles -> 3, 4 -> 5, 5 -> 7, 6+ -> 10.
    manna_for_run: tuple = (0, 1, 2, 3, 5, 7, 10, 10, 10)

    # knowledge per tile in a metal run, indexed by tile type
    # (0..11; manna types are zero). Legacy spells.c table.
    knowledge_per_tier: tuple = (0, 0, 0, 0,      # empty + manna
                                 1, 3, 10, 25,    # bronze..platinum
                                 50, 100, 200, 500)

    # resource caps (fx.manna_cap still overrides per-mode when set)
    manna_cap: int = 99
    knowledge_cap: int = 9999

    def validate(self):
        assert len(self.manna_for_run) == 9, "manna_for_run: 9 entries"
        assert len(self.knowledge_per_tier) == 12, \
            "knowledge_per_tier: 12 entries"
        assert all(0 <= v <= 255 for v in self.manna_for_run)
        assert all(0 <= v <= 65535 for v in self.knowledge_per_tier)
        assert 1 <= self.manna_cap <= 255
        assert 1 <= self.knowledge_cap <= 65535
