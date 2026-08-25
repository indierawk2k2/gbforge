"""Screen assertions over the raw framebuffer (screenshot_raw).

All helpers work on Frame objects — a thin wrapper around the RGBA
bytes from GB.framebuffer() — so no disk I/O is needed for assertions.
Golden comparison and diff artifacts use PNG files under
harness/golden/, encoded and decoded here with stdlib zlib — the
harness has no imaging dependency in either language.
"""

import hashlib
import os
import struct
import zlib

W, H = 160, 144


class Frame:
    def __init__(self, buf, w=W, h=H):
        assert len(buf) == w * h * 4
        self.buf = buf
        self.w = w
        self.h = h

    @classmethod
    def grab(cls, gb):
        w, h, buf = gb.framebuffer()
        return cls(buf, w, h)

    def pixel(self, x, y):
        """(r, g, b) at x,y."""
        i = (y * self.w + x) * 4
        return tuple(self.buf[i:i + 3])

    def hash(self, region=None):
        """Stable content hash of the frame (or a region) — the unit of
        animation traces."""
        if region is None:
            return hashlib.sha1(self.buf).hexdigest()[:16]
        x0, y0, x1, y1 = region
        h = hashlib.sha1()
        for y in range(y0, y1):
            start = (y * self.w + x0) * 4
            h.update(self.buf[start:start + (x1 - x0) * 4])
        return h.hexdigest()[:16]

    def diff_count(self, other, region=None, tolerance=2):
        """Number of differing pixels (per-channel tolerance)."""
        x0, y0, x1, y1 = region or (0, 0, self.w, self.h)
        count = 0
        for y in range(y0, y1):
            for x in range(x0, x1):
                i = (y * self.w + x) * 4
                a, b = self.buf[i:i + 3], other.buf[i:i + 3]
                if any(abs(p - q) > tolerance for p, q in zip(a, b)):
                    count += 1
        return count

    def save_png(self, path):
        """Write the frame as 8-bit RGB PNG using stdlib zlib only.

        Filter 0 on every scanline: the images are 160x144 flat-colour
        Game Boy frames, so filtering buys almost nothing, and a
        deterministic encoder means a golden file only changes when
        the PIXELS change — not when someone's imaging library
        upgrades its heuristics."""
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        raw = bytearray()
        for y in range(self.h):
            raw.append(0)
            row = self.buf[y * self.w * 4:(y + 1) * self.w * 4]
            for x in range(self.w):
                raw += row[x * 4:x * 4 + 3]

        def chunk(tag, data):
            return (struct.pack(">I", len(data)) + tag + data
                    + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

        png = (b"\x89PNG\r\n\x1a\n"
               + chunk(b"IHDR", struct.pack(">IIBBBBB", self.w, self.h,
                                            8, 2, 0, 0, 0))
               + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
               + chunk(b"IEND", b""))
        with open(path, "wb") as f:
            f.write(png)
        return path

    @classmethod
    def load_png(cls, path):
        """Read an 8-bit RGB/RGBA PNG (the only kind save_png writes)."""
        data = open(path, "rb").read()
        assert data[:8] == b"\x89PNG\r\n\x1a\n", f"{path}: not a PNG"
        pos, idat, w, h, depth, color = 8, b"", None, None, None, None
        while pos < len(data):
            ln = struct.unpack(">I", data[pos:pos + 4])[0]
            tag = data[pos + 4:pos + 8]
            body = data[pos + 8:pos + 8 + ln]
            pos += 12 + ln
            if tag == b"IHDR":
                w, h, depth, color = struct.unpack(">IIBB", body[:10])
            elif tag == b"IDAT":
                idat += body
            elif tag == b"IEND":
                break
        assert (w, h) == (W, H), f"{path} is {(w, h)}, want {(W, H)}"
        assert depth == 8 and color in (2, 6), \
            f"{path}: only 8-bit RGB/RGBA PNGs are supported"
        nch = 3 if color == 2 else 4
        stride = w * nch
        raw = zlib.decompress(idat)
        out = bytearray(w * h * 4)
        prev = bytearray(stride)
        i = 0
        for y in range(h):
            ft = raw[i]
            i += 1
            line = bytearray(raw[i:i + stride])
            i += stride
            for x in range(stride):
                a = line[x - nch] if x >= nch else 0
                b = prev[x]
                c = prev[x - nch] if x >= nch else 0
                if ft == 1:
                    line[x] = (line[x] + a) & 0xFF
                elif ft == 2:
                    line[x] = (line[x] + b) & 0xFF
                elif ft == 3:
                    line[x] = (line[x] + ((a + b) >> 1)) & 0xFF
                elif ft == 4:
                    p = a + b - c
                    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                    pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                    line[x] = (line[x] + pr) & 0xFF
            for x in range(w):
                o, s0 = (y * w + x) * 4, x * nch
                out[o:o + 3] = line[s0:s0 + 3]
                out[o + 3] = line[s0 + 3] if nch == 4 else 0xFF
            prev = line
        return cls(bytes(out))


_HARNESS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def expect_screen(gb, golden_name, region=None, tolerance=2,
                  match_pct=99.0, artifacts_dir=None):
    """Compare the live frame against harness/golden/<name>.png.

    Paths resolve against the harness directory, not the working
    directory: a test that only passes when pytest is invoked from
    one particular folder is a test that will fail in CI.

    On failure the actual frame is written next to the golden's name
    so the two can be opened side by side."""
    artifacts_dir = artifacts_dir or os.path.join(_HARNESS, "output",
                                                  "failures")
    golden_path = os.path.join(_HARNESS, "golden", f"{golden_name}.png")
    frame = Frame.grab(gb)
    golden = Frame.load_png(golden_path)
    x0, y0, x1, y1 = region or (0, 0, W, H)
    total = (x1 - x0) * (y1 - y0)
    diff = frame.diff_count(golden, region, tolerance)
    pct = 100.0 * (total - diff) / total
    if pct < match_pct:
        os.makedirs(artifacts_dir, exist_ok=True)
        actual = os.path.join(artifacts_dir, f"{golden_name}_actual.png")
        frame.save_png(actual)
        raise AssertionError(
            f"screen {golden_name}: {pct:.1f}% match "
            f"(< {match_pct}%), {diff}/{total} pixels differ — "
            f"actual saved to {actual}")
    return pct


class Recording:
    """Per-frame hash sequence — the animation-trace primitive."""

    def __init__(self, gb, region=None):
        self.gb = gb
        self.region = region
        self.hashes = []
        self.frames = []

    def capture(self, n=1, keep_frames=False):
        """Advance n frames, hashing each."""
        for _ in range(n):
            self.gb.run_frames(1)
            f = Frame.grab(self.gb)
            self.hashes.append(f.hash(self.region))
            if keep_frames:
                self.frames.append(f)
        return self

    def stable_span(self):
        """Length of the identical-hash run at the end of the recording."""
        if not self.hashes:
            return 0
        last = self.hashes[-1]
        n = 0
        for h in reversed(self.hashes):
            if h != last:
                break
            n += 1
        return n

    def expect_region_stable(self, region=None):
        """Assert the recorded region never changed across the recording.
        (Record with region=... — this checks the hash sequence.)"""
        assert region is None or region == self.region, \
            "record with the region you want to assert on"
        distinct = set(self.hashes)
        if len(distinct) != 1:
            raise AssertionError(
                f"region changed during recording: {len(distinct)} "
                f"distinct states over {len(self.hashes)} frames")
