"""Transport for the automation line protocol.

A transport sends one newline-terminated command and returns the
parsed one-line JSON reply. Higher-level verbs live in pygb.client.GB;
nothing outside this module touches a subprocess directly.

The protocol (harness/PROTOCOL.md) is deliberately transport-agnostic:
one verb-per-line request, one JSON object per reply, no state on the
wire. gbctl — a headless libsameboy binary that only advances the
emulation when told to — is the transport that ships here, because it
is the one that belongs in CI: no window, no wall clock, and the same
frame sequence on every machine.
"""

import json
import os
import subprocess

_HARNESS = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
_REPO_ROOT = os.path.dirname(_HARNESS)

ROM_DEFAULT = os.path.join(_REPO_ROOT, "examples", "cascadia", "cascadia.gbc")
GBCTL_DEFAULT = os.path.join(_HARNESS, "build", "gbctl")
BOOT_ROM_DEFAULT = os.path.join(_HARNESS, "sameboy", "build", "bin",
                                "BootROMs", "cgb_boot.bin")


class StdioTransport:
    """Line protocol over a gbctl child process's stdin/stdout.

    Deterministic by construction: gbctl is parked — emulation advances
    only on an explicit run_frames/step/tap. Lines that aren't JSON
    (the harness banner, SameBoy log noise) are skipped.
    """

    def __init__(self, rom=ROM_DEFAULT, gbctl=None, boot_rom=None):
        gbctl = gbctl or GBCTL_DEFAULT
        if not os.path.exists(gbctl):
            raise RuntimeError(
                f"gbctl not built at {gbctl} — run `make -C harness gbctl`")
        rom = os.path.abspath(rom)
        if not os.path.exists(rom):
            raise RuntimeError(
                f"ROM not built at {rom} — run `make -C examples/cascadia`")
        argv = [gbctl, rom, os.path.abspath(boot_rom or BOOT_ROM_DEFAULT)]
        self.proc = subprocess.Popen(
            argv, cwd=_HARNESS,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, text=True,
        )

    def request(self, cmd):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()
        while True:
            line = self.proc.stdout.readline()
            if not line:
                raise RuntimeError("gbctl exited unexpectedly")
            line = line.strip()
            if line.startswith("{"):
                return json.loads(line)
            # banner / log noise — skip

    def close(self):
        try:
            self.proc.stdin.write("quit\n")
            self.proc.stdin.flush()
        except Exception:
            pass
        try:
            self.proc.wait(timeout=2.0)
        except Exception:
            self.proc.kill()
