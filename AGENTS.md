# Working in this repository

Orientation for coding agents (and humans) seeing this repo for the
first time. Everything here was verified against the tree as written;
if reality and this file disagree, fix this file in the same change.

## What this is

A Python model layer (`gbforge/`) that compiles declarative
puzzle-game specs into C configuration tables, a shared hand-written
Game Boy Color runtime (`runtime/`) that consumes them, and one
complete worked example (`examples/cascadia/`). The README's numbers
are measured from this tree and end up in front of readers who may
verify them — if your change moves a number, re-measure and update.

## Environment setup

Two tools, no Python dependencies:

1. **Python 3.9+** — the codegen and the asset generator are stdlib
   only. `python3 -m gbforge build examples/cascadia -o
   examples/cascadia/generated` should work from the repo root
   immediately.
2. **GBDK-2020 4.5.0+** — the C toolchain (SDCC + lcc + ROM tools).
   Download a release from
   https://github.com/gbdk-2020/gbdk-2020/releases, unpack anywhere,
   and point `GBDK_HOME` at its root (the directory containing
   `bin/lcc`). The path must end with a slash:

   ```bash
   export GBDK_HOME=/opt/gbdk/     # trailing slash matters
   ```

Full build from a fresh clone:

```bash
cd examples/cascadia
make gen        # spec -> generated/   (should be a no-op diff)
make res        # regenerate res/ + art/ (should be a no-op diff)
make            # -> cascadia.gbc, gated by scripts/check_banks.py
make debug      # dev ROM with the scriptable mailbox compiled in
```

`make gen` and `make res` regenerating with **zero diff** is the
first sanity check that your environment matches the committed tree.

To play the ROM, any GBC emulator works. [SameBoy](https://sameboy.github.io/)
and [mGBA](https://mgba.io/) are both accurate enough for this
codebase's timing behavior.

## Test environment

There is no test suite *in* this repository — the private project
this was extracted from verifies the runtime by execution against a
scripted emulator harness, and the example ROM shipped here passed
that checklist (boot, swap-revert, scoring, cascade, win, lose)
before being committed. What this repo gives you is everything needed
to rebuild that kind of loop with any emulator that exposes memory
reads and button injection:

- **Symbols.** Building emits `cascadia.noi` next to the ROM — a
  linker symbol table mapping every non-static C variable to its
  WRAM address. Parse it; never hardcode addresses. Useful anchors in
  the example: `_board` (64 bytes, row-major), `_score` (u16 LE),
  `_moves_left`, `_cursor_x`/`_cursor_y`, `_tile_selected`,
  `_processing_matches` (1 while a resolve is animating).

- **The debug mailbox** (`runtime/debug.h`, dev builds only). A
  4-byte WRAM request block polled by the game loop. Write args
  first, then the request byte; the game clears it when consumed:
  - `debug_req=1` (ENTER_MODE): jump from the title into gameplay.
  - `debug_req=2` (TRIGGER_SWAP): `arg0`=x, `arg1`=y, `arg2`=direction
    (0 up, 1 down, 2 left, 3 right) — performs the swap as if input.
  - `debug_req=3` (REDRAW): copies the `_board` mirror you wrote into
    the engine and repaints — i.e., **board injection**.
  - `debug_rng_force=1` + `debug_rng_seed`: deterministic boards.

  Inject a board, trigger a swap, wait until `_processing_matches`
  returns to 0, read `_score`/`_board`, assert. That is the entire
  verification pattern; it needs no game-specific tooling.

- **Frame geometry is checked, not eyeballed.**
  `scripts/check_frame_geometry.py <shot.png> --rect x0,y0,x1,y1`
  asserts a screenshot's border renders as a clean rectangle: every
  edge continuous, and no line pixels overhanging past a corner —
  the defect class where a corner tile's arm crosses the joint.
  Intentional overlaps (a portrait breaking a dialogue border) are
  declared with `--allow x0,y0,x1,y1`, never silently accepted. The
  committed Cascadia screenshot passes with `--rect 1,1,132,132`
  and zero allows; keep it that way when retaking it.
- **Bank discipline is a build gate.** `scripts/check_banks.py` runs
  on every link and fails the build if any linked area or symbol
  falls outside its ROM bank window. The linker is silent about this
  failure mode; the checker is not optional.

## Rules that keep this repo healthy

- **Never hand-edit machine-made files.** `examples/*/generated/` and
  `examples/*/res/` are outputs. Change the spec (`cascadia.py`), the
  generators (`gbforge/codegen/`), or the asset generator
  (`scripts/gen_placeholder_res.py`), then regenerate and commit the
  outputs together with the source change.
- **Generated code is data, not logic.** Codegen emits `const`
  tables. If a change needs generated control flow, that's a design
  discussion, not a patch.
- **BANKED discipline in the runtime.** Files with `#pragma bank N`
  must declare every public function `BANKED` (in the header too).
  A non-BANKED function in a banked file compiles into a direct,
  un-switched call that works only while its bank happens to be
  mapped — the failure is a wild jump, found the hard way.
- **Hardware constraints are real:** no malloc, no float, no
  division in per-frame paths (lookup tables instead), `uint8_t` for
  tile values, VRAM writes batched into blanking windows.
- **No new dependencies** — Python stdlib and GBDK only. If a task
  seems to need one, stop and surface it.
- **No borrowed assets.** Everything visual ships from
  `gen_placeholder_res.py`. Nothing from commercial ROMs, other
  projects, or unlicensed libraries goes in this tree.
- **Measure, don't estimate.** Any count destined for the README
  (`wc -l`, file counts) is measured from the tree at commit time.

## Layout

```
gbforge/            the model, codegen, and pure-Python reference sim
  model/            dataclasses a spec instantiates (Game, Mode, ...)
  codegen/          one emitter per concern -> const C tables
  engine/sim.py     reference board engine (mirrors runtime/engine.c)
runtime/            shared C runtime; hot paths hand-written
examples/cascadia/  spec + glue + generated output + ROM, all committed
scripts/            asset generator, bank checker
```

The asset contract between `res/` and the runtime (tile indices,
sprite slots, palette numbers) is defined by the generated headers in
`examples/cascadia/res/*.h` — the runtime compiles against those
names, and `gen_placeholder_res.py` is their single source.
