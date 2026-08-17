# Cascadia

A complete match-3 defined by [cascadia.py](cascadia.py) — 44 lines of
declarative spec — plus [main_cascadia.c](main_cascadia.c), the thin
per-game loop that wires the generated tables to the shared runtime.

**Rules:** four gem kinds on an 8×8 board. Move the cursor with the
D-pad, A grabs, a direction swaps. Swaps only commit when they match;
matched tiles score 10 points each, tiles fall, the board refills from
the top, and chains keep resolving (and scoring) until stable. 1000
points wins; 30 moves is all you get.

**What to look at, in order:**

1. `cascadia.py` — every tuning knob the game has.
2. `generated/` — what gbforge emits from it: pure `const` data.
3. `main_cascadia.c` — the wiring; note how little game is in it.
4. `screenshot.png` — the result running in an emulator
   (`screenshot@3x.png` is the same frame scaled for reading).

The art in `res/` and `art/` is generated placeholder
(`../../scripts/gen_placeholder_res.py`): flat-colored gems with
distinct shapes so kind is readable without color.

## Build

```bash
make gen                        # regenerate generated/ from the spec
make res                        # regenerate the placeholder art pack
GBDK_HOME=/path/to/gbdk/ make   # link cascadia.gbc
make debug                      # dev ROM with the scriptable mailbox
```

The committed `cascadia.gbc` is the retail build of exactly this tree.
