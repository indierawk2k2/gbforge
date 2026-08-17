"""AnimationSpec — every animation timing as declarative data.

These classes describe WHAT the animations do; gen_anim emits the ROM
parameter table the runtime interpreter reads (plus the DEBUG-build
WRAM mirror and the JSON manifest the animation editor's sliders bind
to). Defaults are the legacy-measured values from render.c.
"""

from dataclasses import dataclass, field

# Legacy swap offset curve (render.c:1589): cumulative pixel offsets
# per frame, reaching 16 px over 12 frames with a 1,1,2-repeat feel.
LEGACY_SWAP_CURVE = (1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 16)


@dataclass
class SwapAnim:
    """Sprite-based tile slide. curve = cumulative offsets per frame;
    its length is the duration and its last entry must be 16 (one
    full tile) for the handoff to the BG blit to be seamless."""
    curve: tuple = LEGACY_SWAP_CURVE

    def validate(self):
        assert len(self.curve) <= 16, "swap curve longer than 16 frames"
        assert self.curve[-1] == 16, "swap curve must end at 16 px"


@dataclass
class FlashAnim:
    hold: int = 8            # frames the matched cells stay flashed


@dataclass
class SparkleSpec:
    phase_frames: int = 6    # frames per burst phase (3 phases)


@dataclass
class WiggleSpec:
    """Hint nudge: the tile jiggles ±1px diagonally (legacy 6
    positions x 5 frames = 30). offsets must end at 0 (rest)."""
    step_frames: int = 5
    offsets: tuple = (1, -1, 1, -1, 1, 0)

    def validate(self):
        assert len(self.offsets) <= 8 and self.offsets[-1] == 0


@dataclass
class GravityAnim:
    delays: tuple = (3, 2, 2, 1, 1, 1, 1, 1)  # frames per fall step


@dataclass
class CascadeTiming:
    clear_hold: int = 6      # beat after tiles vanish
    pass_gap: int = 5        # between cascade passes


@dataclass
class ShakeSpec:
    """±1px diagonal screen shake via SCX/SCY (legacy render.c
    tables). dx/dy are signed per-frame offsets; length = duration.
    Mode configs decide WHEN it fires (shake_run / shake_passes)."""
    dx: tuple = (-1, 1, -1, 1, -1, -1, 0)
    dy: tuple = (-1, -1, 1, 1, -1, 1, 0)

    def validate(self):
        assert len(self.dx) == len(self.dy) <= 8
        assert self.dx[-1] == 0 and self.dy[-1] == 0, \
            "shake must return to rest"


@dataclass
class BoardAnimations:
    swap: SwapAnim = field(default_factory=SwapAnim)
    flash: FlashAnim = field(default_factory=FlashAnim)
    gravity: GravityAnim = field(default_factory=GravityAnim)
    timing: CascadeTiming = field(default_factory=CascadeTiming)
    shake: ShakeSpec = field(default_factory=ShakeSpec)
    sparkle: SparkleSpec = field(default_factory=SparkleSpec)
    wiggle: WiggleSpec = field(default_factory=WiggleSpec)
