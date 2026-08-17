# gbforge

A declarative model for tile-based puzzle games that compiles to Game
Boy Color code at build time.

You describe the game — modes, rules data, animation timings, UI,
win/lose overlays — in about forty lines of Python. gbforge turns
that into the C configuration tables a shared, hand-tuned runtime
consumes, and the result links into a ROM that runs on an 8-bit CPU
with 8KB of work RAM and bank-switched ROM.

## Why this exists

The Game Boy Color has no floating point, measures its budgets in
cycles per scanline, and silently drops VRAM writes that land outside
the blanking windows. Historically that pushes you to write the game
*as* low-level code, because the machine can't afford the indirection
a higher-level model would cost at runtime.

That constraint binds at runtime, not at build time. gbforge resolves
the abstraction while compiling: the model becomes constant tables,
and the runtime that interprets them is written once, by hand, against
the hardware's real timing windows. The hardware never pays for the
expressiveness; adding a game costs a spec, not an engine.

The reason to do this now is economic. With coding agents doing
implementation under specification and review, one person can carry a
model layer, a code generator, a runtime, a scriptable emulator test
harness, and a shipping game simultaneously — a scope that previously
wasn't worth attempting solo. Cheap implementation moves the correct
abstraction boundary up; this repository is what that looks like in
practice.

## The example

`examples/cascadia/` is a complete match-3: swap-to-match, gravity,
refills, cascade chains, 10 points a tile, win at 1000, lose after 30
moves. The gameplay-shaped part is [44 lines of Python](examples/cascadia/cascadia.py):

```python
GAME = Game(
    name="cascadia",
    modes=[
        Mode("classic", refill=True, shake_run=4, shake_passes=2),
    ],
    overlays=[
        Overlay("win", pos=(4, 6), size=(12, 5),
                lines=[(1, "YOU WIN"), (2, "1000 POINTS")]),
        Overlay("lose", pos=(4, 6), size=(12, 5),
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
        logo_letters=8,
        deco_palettes=("fire", "earth", "gold", "silver") * 3,
    ),
)
```

Every number above is live gameplay data: the swap slide follows that
six-entry pixel curve, gravity accelerates on those per-step delays,
the screen shakes on a four-run. Change a value, rebuild, and the ROM
plays differently — no engine code touched.

What it builds from, measured:

| Piece | Size | Written by |
|---|---|---|
| `cascadia.py` — the spec | 44 lines | you |
| `main_cascadia.c` — input edges, score/move counters, win ladder | 281 lines | you (once per game; `gen_main` is the roadmap) |
| `generated/` — config tables from the spec | 130 lines, 8 files | gbforge |
| `res/` — placeholder art pack (tiles, font, palettes, sprites) | 478 lines | `scripts/gen_placeholder_res.py` |
| `runtime/` — the shared engine | 2,425 lines, 13 files | written once, shared by every game |

The generated output and the built ROM are **committed on purpose**,
so you can read the before and after without installing a toolchain:
spec → [generated tables](examples/cascadia/generated/) →
[screenshot](examples/cascadia/screenshot.png).

## How it works

```
spec (Python)  →  model objects  →  codegen  →  C tables  ─┐
                                                            ├─→  GBDK/SDCC  →  ROM
      hand-written runtime (engine, animator, UI, VRAM)  ──┘
```

| Stage | What happens |
|---|---|
| **Model** | Plain dataclasses: modes, overlays with box geometry, animation curves, ability opcodes, audio event maps. Validated at build time. |
| **Codegen** | Each generator emits one concern as `const` C tables — no control flow is generated, only data the runtime indexes. |
| **Runtime** | A resolution-script engine: the board logic emits an event list (matches, falls, refills, transmutes) and a separate interpreter plays it against the data-driven timings. Rendering batches VRAM writes into the blanking windows; a staged row-blast path handles the frames where a naive write would tear. |
| **Reference sim** | The same engine semantics exist in pure Python (`gbforge/engine/sim.py`), kept honest by a 10,000-board transcript hash against the C implementation. |

Two design rules carry most of the weight:

- **Generated code is data, not logic.** The runtime's hot paths are
  hand-written against the hardware; the model can grow richer without
  the ROM getting slower.
- **Behavior is verified by execution.** The private project this was
  extracted from drives every change through a scriptable emulator
  harness — board states injected over a debug mailbox, frames
  compared, RNG streams checked byte-for-byte. The example ROM in this
  repository passed the same style of scripted checklist (swap-revert,
  scoring, cascades, win/lose) before it was committed.

## Development setup

[AGENTS.md](AGENTS.md) is the onboarding document — environment
setup, how to rebuild the scripted verification loop against any
emulator with memory access (symbol table + debug mailbox), and
the rules that keep the tree healthy. It's written for coding
agents and works just as well for people.

## Building the example

Requires [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020) 4.5.0+
and Python 3.9+. No Python dependencies.

```bash
cd examples/cascadia
make gen                      # cascadia.py -> generated/  (committed)
make res                      # regenerate the placeholder art pack
GBDK_HOME=/path/to/gbdk/ make # -> cascadia.gbc
```

Output: `examples/cascadia/cascadia.gbc`, runnable in any Game Boy
Color emulator. `make debug` builds a dev ROM with a scriptable
mailbox (`runtime/debug.h`) that a test harness can drive.

## Scope

gbforge is extracted from a larger private project — a complete
commercial-shape GBC game whose entire mode set (endless, puzzle,
timed, battle with a CPU opponent, a quest campaign with dialogue,
a store, battery saves) runs on this runtime, byte-for-byte
equivalent to the hand-written implementation it replaced. That game
and its assets are not part of this repository; the example art is
generated placeholder by design.

This is working code, not a product. The model grows when the games
need it to, and there is no stability guarantee.

## License

Apache License 2.0 — see [LICENSE](LICENSE).

Copyright Emerald City Modulation LLC.
