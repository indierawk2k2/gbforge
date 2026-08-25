# Automation line protocol

One protocol, one transport in this repository:

| Transport | Binary | Framing | Timing model |
|---|---|---|---|
| **gbctl** (`harness/build/gbctl`) | headless CLI linking libsameboy | stdin/stdout lines | Parked: emulation advances **only** on `run_frames`/`step`/`tap`. Deterministic; CI-safe. |

The protocol is deliberately dumb — a verb per line in, one JSON
object per line out, no session state on the wire — because that is
what makes a second transport cheap. The private project this was
extracted from runs the same commands over a Unix socket into a
patched SameBoy.app, so a scenario written here can be replayed with a
window open and a human watching, without the scenario knowing.

Every command is a single `\n`-terminated ASCII line: `verb [args...]`.
Every reply is a single `\n`-terminated JSON object. Success replies
include `"ok":true`; failures are `{"ok":false,"error":"..."}`. Numeric
arguments accept decimal or `0x` hex.

The Python client (`harness/pygb`) is the only supported caller. It
chunks around per-command byte caps and never hand-types addresses:
symbols come from the `.noi` of the ROM actually loaded.

## Execution

| Command | Reply extras | Notes |
|---|---|---|
| `run_frames <n>` | `frames` | Runs exactly n frames. |
| `step` | `frames` | One frame. |
| `run_to_vblank` | `frames` | Completes the current frame. |
| `quit` | | Exits. The reply may not arrive. |
| `pause` / `resume` / `set_speed <mult>` | | Accepted no-ops. gbctl is always parked and always unthrottled; a free-running transport needs them, and a scenario that works on both must be able to send them. |

## Input

Buttons: `A B UP DOWN LEFT RIGHT START SELECT` (player 0).

| Command | Notes |
|---|---|
| `press <btn>` / `release <btn>` | Latching key state. |
| `tap <btn> [hold]` | Press, run `hold` frames (default 2), release, run 2 frames. |

## Memory

| Command | Reply extras | Notes |
|---|---|---|
| `read <addr>` | `value` | `GB_safe_read_memory`. |
| `read_block <addr> <count>` | `data` (hex string) | count ≤ 256 per call. |
| `write_byte <addr> <val>` | | `GB_write_memory`. |
| `write_block <addr> <hex>` | `count` | hex string, ≤ 256 bytes per call. |
| `read_vram <bank> <addr> <count>` | `data` (hex string) | bank 0/1, addr 0x8000–0x9FFF, count ≤ 256. Direct access — immune to PPU locking. |
| `snapshot <addr> <w> <h>` | `rows` (array of arrays) | Reads w×h bytes at addr as a grid. Game-agnostic; addresses come from pygb. |

## PPU state (direct access, bypasses CRAM/OAM locks)

Register-mediated reads return `0xFF` while the LCD has CRAM or OAM
locked during PPU mode 2/3, which silently turns a real assertion into
a passing one. These read SameBoy's internal buffers instead.

| Command | Reply extras |
|---|---|
| `read_obj_palette <slot 0-7> <color 0-3>` | `r g b` (5-bit each) |
| `read_bg_palette <slot 0-7> <color 0-3>` | `r g b` |
| `read_oam <slot 0-39>` | `y x tile attr` |

## Savestates

| Command | Notes |
|---|---|
| `save_state <path>` | Absolute paths recommended (pygb sends absolute). |
| `load_state <path>` | States are ROM-build-specific; the checkpoint cache keys them by ROM hash. |

## Capture

Pixel wire format: **RGBA8888 bytes, row-major, top-left origin,
160×144.** PNG encoding happens in `pygb.screen` (stdlib `zlib`), not
here — one encoder, and the harness links no image library.

| Command | Reply extras | Notes |
|---|---|---|
| `screenshot_raw` | `w h format data` | Frame inline as base64 RGBA8888. No disk I/O, no emulation advance. |
| `regs` | `pc sp bank` | CPU program counter, stack pointer, mapped ROM bank — hang diagnosis. |

## ROM

| Command | Notes |
|---|---|
| `load_rom <path>` | Loads a new ROM and resets. A/B testing two builds in one session (a detector run against the pre-fix and post-fix ROM) is the reason this exists. |
