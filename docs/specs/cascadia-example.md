# Agent brief: build the `cascadia` worked example

*This is the task specification `examples/cascadia/` was built from,
published as-is. It is here as a worked example of how work gets
handed off in this repository, not as a template — the shape that
matters is: goal, non-goals with their reasons, a table of decisions
already made, a verification checklist, and a definition of done.*

*Two notes, since a reader will find both:*

*The brief asks for a 6×6 board and the example ships 8×8. The
runtime's board dimensions are compile-time constants (`RT_W`, `RT_H`
in `runtime/engine.h`), so 6×6 was not expressible without changing
the engine — which constraint 5 below forbids, and instructs to report
instead. The gap surfacing as information rather than as a quiet
widening of scope is the constraint working.*

*The spec targets "under 40 lines"; `cascadia.py` is 44. Measured, not
rounded down.*

---

## Context

This project is a Python abstraction layer for tile-based puzzle games that compiles to Game Boy Color code at build time. It is being extracted from a larger private project into a public repository. It needs one complete, readable worked example that demonstrates the whole pipeline from declarative spec to running ROM.

You are also establishing this repository's naming, package layout, and module conventions — that work is yours to decide. Note that **"Cascadia" is the name of the example game only**; do not use it as the project or package name.

## Goal

Produce `examples/cascadia/` — a complete, buildable match-3 game defined entirely by a spec in this project's DSL, with the generated output committed alongside it.

## Non-goals and hard constraints

Not negotiable. Violating any of these makes the work unusable.

1. **Do not reference, import, copy, or adapt anything from the private game this project was extracted from.** No mechanics, no rule definitions, no art, no level data, no naming. Cascadia is a plain match-3 and must be independently specified.
2. **Do not use any art assets from the private game.** Generate new placeholder sprites (see Deliverables). Four flat-colored 16×16 tiles with distinct shapes are sufficient and preferable — placeholder art should look deliberately like placeholder art.
3. **Do not add dependencies** without asking. If the task appears to need one, stop and report why.
4. **Do not modify the IR contract or the memory-plan format.** If the example can't be expressed without changing them, stop and report the gap — that's a design finding, not a blocker to route around.
5. **Do not modify the core engine to make the example work.** If the backend can't emit something the spec requires, report it. A missing capability discovered by the example is a useful result.

## The game to build

Standard match-3. Nothing clever — the example's job is to be instantly legible so the reader's attention goes to the abstraction, not the game.

| Element | Value |
|---|---|
| Board | 6 × 6, 16px cells |
| Tile kinds | 4, visually distinct |
| Input | Cursor moves with D-pad; A selects; A on an adjacent selected tile swaps |
| Swap validity | A swap is only committed if it produces at least one match; otherwise it reverts |
| Match | 3 or more of the same kind in a row or column |
| Clear | Matched tiles are removed; 10 points each |
| Gravity | Tiles above a cleared cell fall down |
| Refill | New random tiles enter from the top edge |
| Cascade | After gravity settles, re-run match detection; chains keep resolving until stable |
| Win | Reach 1000 points |
| Lose | Exhaust 30 moves |

A move counts when a swap is committed, not when the cursor moves.

**The initial board must not start with a match already on it.** Reroll at generation time until stable.

## Deliverables

```
examples/cascadia/
  README.md              Short. What the example is, how to build it, what to look at.
  cascadia.py            The complete game spec. Target: under 40 lines.
  art/
    ruby.png             16×16 placeholder, 4-color GBC palette
    emerald.png
    topaz.png
    onyx.png
  generated/             Committed build output — see note below
    *.{c,h,asm}
  cascadia.gbc           Built ROM
  screenshot.png         Emulator capture of actual gameplay, not a title screen
```

**The generated directory is committed intentionally.** The point of the example is the before/after contrast, and a reader shouldn't need a toolchain to see it. Add a header comment to each generated file marking it as machine-generated and naming the spec it came from.

## Verification — required, not optional

Do not report this task complete on the basis of the code looking correct.

1. The build command for this project completes with no errors against `examples/cascadia`.
2. The generated source builds to a ROM with the project toolchain.
3. The ROM boots in an emulator and reaches a playable board.
4. Drive the following through the emulator harness and confirm each:
   - A swap that produces no match reverts, and does not decrement the move counter.
   - A horizontal 3-match clears and scores 30.
   - A vertical 3-match clears and scores 30.
   - Tiles above a cleared cell fall; empty cells refill from the top.
   - A cascade chain resolves fully and scores each stage.
   - Reaching 1000 points triggers the win state.
   - Exhausting 30 moves triggers the lose state.
5. Capture `screenshot.png` from a real frame during play.

If any check fails, fix it and re-run the whole list. **Report which checks you actually executed versus reasoned about** — do not describe a check as passing unless you ran it and saw the result.

## Report back with

- The spec's line count, and the generated output's total line and file counts. These numbers go in the top-level README, so they must be measured.
- Any capability the backend was missing.
- Anything in the spec DSL that felt awkward to express. The example is also a usability test of the model.
- The verification checklist with actual pass/fail per item.
- The naming and package-layout conventions you settled on, and why.

## Definition of done

A reader can open `examples/cascadia/cascadia.py`, then `examples/cascadia/generated/`, and understand in under a minute what the abstraction layer is doing. The ROM runs. Every number in the report is measured. Nothing in the tree is half-finished.
