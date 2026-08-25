# sprite-editor

A SwiftUI macOS app for drawing the game's 16×16 tiles and editing its
8 background palettes, reading and writing `res/*.c` directly.

```bash
cd tools/sprite-editor && ./build-app.sh    # -> GBSpriteEditor.app
open GBSpriteEditor.app                     # File > Open the game's res/ directory
```

## Why it edits C source

There is no intermediate project format. The editor's document *is*
`examples/cascadia/res/tiles_data.c` and `res/palettes.c` — it parses
them on open and rewrites them on save, and the build compiles exactly
what was saved.

That removes the failure this kind of pipeline usually has: an export
step that someone forgets to run, leaving the art in the tool and the
old art in the ROM. There is nothing to forget, because there is only
one copy.

The cost is a format contract with two enforcement points:

- `scripts/editor-roundtrip.sh` compiles the editor's UI-free
  Models + Services with `swiftc` and runs import → export → import →
  export headlessly. The two exports must be byte-identical. This runs
  in CI without a window server.
- `tests/test_asset_contracts.py` checks the boundary from the other
  side: that the arrays the runtime links against are still exported,
  still unsized (the importer greps for `name[]`), that palettes still
  use the `RGB()` macro with `// Name (N)` slot comments, and — the
  one that bit for real — that **no symbol the editor doesn't
  round-trip lives in a file the editor rewrites.**

That last rule is why `sprite_palettes` lives in the generated
`res/sprites_data.c` while `bg_palettes` lives in the editor-owned
`res/palettes.c`. The first real export over `res/` produced a ROM
that would not link, because the editor faithfully rewrote
`palettes.c` with only the symbol it knew about.

## Ownership

| File | Owner |
|---|---|
| `tiles_data.c`, `palettes.c`, `spell_icons.c` | the editor. `make res` seeds them once and never overwrites them again (`--force` to insist). |
| `ui_tiles.c`, `sprites_data.c` | `scripts/gen_placeholder_res.py` |
| `ghost_tiles.c` | derived from `tiles_data.c` on every build by `scripts/derive_ghost_tiles.py` — redraw a gem, rebuild, and its faded refill variant follows |
| `*.h` | `gen_placeholder_res.py` — the index constants both sides compile against |
| `indicator_tiles.c` | the editor, for games that use indicator tiles. Cascadia does not, so nothing compiles it; it is committed because the committed `res/` is exactly what the tool produces, and a file the editor writes on every save should not show up as a spurious diff. |

The private project this was extracted from runs the same pattern for
three more editors (portraits, sound effects, a tile/pixel editor),
each with its own round-trip gate against the same `res/` tree.
