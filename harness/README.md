# harness — the verification loop

`gbctl` is SameBoy's emulator core linked into a headless binary that
speaks one line protocol (see [PROTOCOL.md](PROTOCOL.md)). Emulation is
**parked**: it advances only when a command says so, so a scripted run
produces the same frames on every machine.

```bash
make -C harness gbctl     # fetch + build SameBoy, build gbctl (first run ~1 min)
make -C harness test      # run the scenario suite
```

What a scenario can do, without the game knowing it is under test:

- **Jump anywhere.** The dev ROM carries a 4-byte WRAM mailbox
  (`runtime/debug.h`). Write args, write the request byte, and the game
  consumes it at a safe point: enter a mode from the title, force the
  RNG seed, teleport the cursor and perform a swap, or redraw after the
  harness has rewritten `board[][]` directly. No button choreography to
  break when a menu gains a row.
- **Read and write any state by name.** Addresses come from the
  linker's `.noi`, parsed per-ROM — never hand-typed. `gb.read_sym`,
  `gb.write_block`, `gb.read_vram`, `gb.oam`, `gb.bg_pal`.
- **See the screen.** `screenshot_raw` returns the framebuffer inline;
  `pygb.screen` compares against `golden/`, hashes regions, and records
  per-frame hash sequences so a one-frame flicker that repairs itself
  is still visible.
- **Start from a checkpoint.** `checkpoints/recipes/*.py` are *recipes*,
  not committed savestates. The cache is keyed by the ROM's SHA1, so a
  rebuilt ROM cannot silently load a stale state — the old directory
  simply stops being used.

The suite runs in about a second because almost nothing replays boot:
`title.py` and `classic_open.py` are generated once, and every scenario
loads one.
