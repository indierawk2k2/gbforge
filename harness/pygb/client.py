"""GB — the one client class for driving the ROM under emulation.

One method per protocol verb, plus symbol-aware helpers. Raw addresses
never appear in calling code: use read_sym/write_sym/addr with names
from the generated pygb.symbols module.
"""

import os

import base64

from . import symload
from .transport import StdioTransport, ROM_DEFAULT

BUTTONS = ("A", "B", "UP", "DOWN", "LEFT", "RIGHT", "START", "SELECT")


class CommandError(RuntimeError):
    """A protocol command returned ok=false."""


class GB:
    def __init__(self, transport, ram=None):
        self.t = transport
        self._ram = ram

    # ── construction ────────────────────────────────────────────────

    @classmethod
    def launch_headless(cls, rom=ROM_DEFAULT, gbctl=None, boot_rom=None):
        """Spawn a gbctl child process — deterministic, no window, CI-safe.

        Binds the symbol table of the ROM being loaded, not the
        default build's: a dev ROM carries the debug mailbox, which
        shifts every variable declared after it. Resolving names
        against the wrong .noi reads plausible-looking garbage from
        whatever moved into the old address, so the binding is done
        here rather than left to each caller."""
        return cls(StdioTransport(rom=rom, gbctl=gbctl, boot_rom=boot_rom),
                   ram=_ram_for(rom))

    # ── protocol plumbing ───────────────────────────────────────────

    def cmd(self, line):
        """Send a raw command line; raise CommandError on ok=false."""
        r = self.t.request(line)
        if isinstance(r, dict) and r.get("ok") is False:
            raise CommandError(f"{line!r} failed: {r}")
        return r

    def close(self):
        self.t.close()

    # ── symbols ─────────────────────────────────────────────────────

    @property
    def ram(self):
        """{name: address} for the ROM currently loaded."""
        if self._ram is None:
            from . import symbols  # generated; ImportError = not built yet
            self._ram = symbols.RAM
        return self._ram

    def addr(self, name):
        """Address of a RAM data symbol by its C-source name."""
        try:
            return self.ram[name]
        except KeyError:
            raise KeyError(
                f"no RAM symbol {name!r} in the loaded ROM — a dev-only "
                f"variable needs the debug build") from None

    def read_sym(self, name):
        return self.read(self.addr(name))

    def read_sym_word(self, name):
        """Little-endian uint16 at a RAM symbol."""
        a = self.addr(name)
        return self.read(a) | (self.read(a + 1) << 8)

    def write_sym(self, name, value):
        self.write(self.addr(name), value)

    # ── execution ───────────────────────────────────────────────────

    def run_frames(self, n):
        self.cmd(f"run_frames {n}")

    def step(self):
        self.cmd("step")

    def quit(self):
        try:
            self.t.request("quit")
        except Exception:
            pass  # the app exits without replying

    def wait_until(self, pred, timeout_frames=600, step_frames=1):
        """Advance emulation until pred(self) is truthy. Returns frames
        elapsed; raises TimeoutError past timeout_frames."""
        elapsed = 0
        while elapsed < timeout_frames:
            if pred(self):
                return elapsed
            self.run_frames(step_frames)
            elapsed += step_frames
        raise TimeoutError(f"wait_until: predicate false after {timeout_frames} frames")

    # ── input ───────────────────────────────────────────────────────

    def press(self, button):
        self.cmd(f"press {button}")

    def release(self, button):
        self.cmd(f"release {button}")

    def tap(self, button, hold_frames=None):
        if hold_frames is None:
            self.cmd(f"tap {button}")
        else:
            self.cmd(f"tap {button} {hold_frames}")

    def play(self, script):
        """Run a space-separated input script, e.g.
        "tap:A wait:10 tap:RIGHT press:B wait:5 release:B"."""
        for tok in script.split():
            verb, _, arg = tok.partition(":")
            if verb == "wait":
                self.run_frames(int(arg))
            elif verb == "tap":
                self.tap(arg)
            elif verb == "press":
                self.press(arg)
            elif verb == "release":
                self.release(arg)
            else:
                raise ValueError(f"unknown script token: {tok!r}")

    # ── memory ──────────────────────────────────────────────────────

    def read(self, addr):
        return self.cmd(f"read 0x{addr:04X}")["value"]

    def write(self, addr, value):
        self.cmd(f"write_byte 0x{addr:04X} 0x{value:02X}")

    def read_block(self, addr, count):
        """Read `count` bytes, chunking around the protocol's 256-byte cap."""
        out = []
        remaining = count
        a = addr
        while remaining > 0:
            n = min(remaining, 256)
            hex_data = self.cmd(f"read_block 0x{a:04X} {n}")["data"]
            out.extend(int(hex_data[i:i + 2], 16) for i in range(0, n * 2, 2))
            a += n
            remaining -= n
        return out

    def read_board(self):
        """The 8x8 board as a list of 8 rows of tile-type bytes."""
        flat = self.read_block(self.addr("board"), 64)
        return [flat[r * 8:(r + 1) * 8] for r in range(8)]

    # ── PPU state (direct-access reads that bypass LCD locks) ───────

    def obj_pal(self, slot, color):
        r = self.cmd(f"read_obj_palette {slot} {color}")
        return (r["r"], r["g"], r["b"])

    def bg_pal(self, slot, color):
        r = self.cmd(f"read_bg_palette {slot} {color}")
        return (r["r"], r["g"], r["b"])

    def read16(self, addr):
        """Little-endian 16-bit read."""
        return self.read(addr) | (self.read(addr + 1) << 8)

    def write16(self, addr, value):
        """Little-endian 16-bit write."""
        self.write(addr, value & 0xFF)
        self.write(addr + 1, (value >> 8) & 0xFF)

    def oam(self, slot):
        """(Y, X, tile, attr) for OAM slot 0-39, from SameBoy's internal
        buffer (GB_safe_read returns 0xFF during PPU mode 2/3)."""
        r = self.cmd(f"read_oam {slot}")
        return (r["y"], r["x"], r["tile"], r["attr"])

    # ── savestates / capture ────────────────────────────────────────

    def save_state(self, path):
        self.cmd(f"save_state {os.path.abspath(path)}")

    def load_state(self, path):
        self.cmd(f"load_state {os.path.abspath(path)}")

    def screenshot(self, path):
        """Write the last completed frame to `path` as PNG (no
        emulation advance). Encoding happens here, not in gbctl —
        one encoder, stdlib only."""
        from .screen import Frame
        return Frame.grab(self).save_png(path)

    def framebuffer(self):
        """The frame as bytes: RGBA8888, row-major, 160x144."""
        r = self.cmd("screenshot_raw")
        assert r["format"] == "rgba8888", r["format"]
        return r["w"], r["h"], base64.b64decode(r["data"])

    # ── extended memory / rom (gbctl + updated fork) ────────────────

    def write_block(self, addr, data):
        """Write bytes, chunking around the 256-byte protocol cap."""
        data = bytes(data)
        off = 0
        while off < len(data):
            chunk = data[off:off + 256]
            self.cmd(f"write_block 0x{addr + off:04X} {chunk.hex().upper()}")
            off += len(chunk)

    def read_vram(self, bank, addr, count):
        out = []
        a = addr
        remaining = count
        while remaining > 0:
            n = min(remaining, 256)
            hex_data = self.cmd(f"read_vram {bank} 0x{a:04X} {n}")["data"]
            out.extend(int(hex_data[i:i + 2], 16) for i in range(0, n * 2, 2))
            a += n
            remaining -= n
        return out

    def snapshot(self, addr, w, h):
        """w*h bytes at addr as a list of h rows."""
        return self.cmd(f"snapshot 0x{addr:04X} {w} {h}")["rows"]

    def load_rom(self, path):
        """Load a different ROM and rebind its symbols — A/B testing a
        pre-fix and post-fix build in one session is the point."""
        self.cmd(f"load_rom {os.path.abspath(path)}")
        self._ram = _ram_for(path)


def _ram_for(rom):
    """RAM symbol table from the .noi beside a ROM, or None if the
    linker map isn't there (then the generated module is the
    fallback)."""
    try:
        return symload.for_rom(rom).ram
    except OSError:
        return None
