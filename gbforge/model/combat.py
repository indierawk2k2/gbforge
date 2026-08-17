"""AbilityDef — spells, and any future game's weapons, as one
machinery with different data.

An Ability is a cost (manna pools), a targeting mode, a target
filter, and an effect opcode the runtime interpreter (ability.c)
applies to the board. The original game's seven spells are all single
opcodes; a space game's weapons would define the same shapes with
different names, costs, and icons.
"""

from dataclasses import dataclass, field

# Targeting modes
TARGET_NONE = 0        # untargeted (cast immediately)
TARGET_TILE = 1        # B enters targeting, A casts at cursor
TARGET_SWAP_PAIR = 2   # two selections (legacy Mercury Slide)

# Target filters
FILTER_ANY_NONEMPTY = 0
FILTER_MANNA = 1
FILTER_METAL_PROMOTABLE = 2
FILTER_NONE = 3        # untargeted abilities

# Effect opcodes (runtime ability.c interpreter)
OP_DESTROY_TILE = 0    # target -> EMPTY
OP_DESTROY_ROW = 1     # target's row -> EMPTY
OP_DESTROY_TYPE = 2    # every tile of target's type -> EMPTY
OP_CONVERT_TILE = 3    # target -> arg (tile id)
OP_PROMOTE_TILE = 4    # target -> target + 1
OP_SHUFFLE_MANNA = 5   # Fisher-Yates over manna tiles (rng)
OP_FREE_SWAP = 6       # unconditional swap (cost check only here)


@dataclass
class Ability:
    name: str
    cost: dict = field(default_factory=dict)  # {"fire": n, ...}
    targeting: int = TARGET_TILE
    target_filter: int = FILTER_ANY_NONEMPTY
    op: int = OP_DESTROY_TILE
    op_arg: int = 0
    unlock_cost: int = 0   # knowledge price in the store (0 = not sold)
    cast_sfx: str = None   # SFX_* name played on cast (None = generic)

    def cost_of(self, pool):
        return self.cost.get(pool, 0)
