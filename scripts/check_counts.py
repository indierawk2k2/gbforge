#!/usr/bin/env python3
"""Keep the README's measured numbers equal to the tree.

The README claims exact line and file counts, and says they are
measured. That is only true for as long as somebody remembers to
re-measure — which, in the middle of a run of runtime changes, nobody
does. Every count in that file has been stale at least once.

So this is the same move the bank checker makes: take a rule that
asked a person to remember and give the job to the build.

    check_counts.py            report the tree's counts, and diff them
                               against the README (exit 1 on drift)
    check_counts.py --write    update the README in place

Numbers live in two places and both are handled. Table cells are found
by the path in their first column and only their digits are rewritten,
so the surrounding prose ("lines of C", "lines of Swift") survives.
Sentences are matched by explicit patterns below — if you add a claim
to the README, add it here or it will silently rot.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
README = os.path.join(ROOT, "README.md")


# ── measuring ────────────────────────────────────────────────────────

# Keyed by the path as it appears in the README's first table column.
COMPONENTS = {
    "cascadia.py":            ("examples/cascadia/cascadia.py",),
    "main_cascadia.c":        ("examples/cascadia/main_cascadia.c",),
    "generated/":             ("examples/cascadia/generated/*.c",
                               "examples/cascadia/generated/*.h"),
    "runtime/":               ("runtime/*.c",),
    "gbforge/":               ("gbforge/**/*.py",),
    "harness/":               ("harness/src/*.c", "harness/include/*.h",
                               "harness/pygb/*.py", "harness/scenarios/*.py",
                               "harness/scripts/*.py",
                               "harness/checkpoints/recipes/*.py"),
    "scripts/":               ("scripts/*.py", "scripts/*.sh"),
    "tests/":                 ("tests/*.c", "tests/*.py"),
    "tools/sprite-editor/":   ("tools/sprite-editor/**/*.swift",),
    "examples/cascadia/res/": ("examples/cascadia/res/*.c",
                               "examples/cascadia/res/*.h"),
}

# harness/include/gbf_symbols.h and pygb/symbols.py are generated from
# the ROM's linker output and gitignored, so they are absent on a clean
# clone and present after a build. Counting them would make the check
# pass or fail depending on whether you had built yet.
EXCLUDE = ("gbf_symbols.h", "symbols.py")


def measure():
    out = {}
    for name, globs in COMPONENTS.items():
        import glob
        paths = []
        for g in globs:
            paths += sorted(glob.glob(os.path.join(ROOT, g), recursive=True))
        paths = [p for p in paths
                 if os.path.isfile(p) and os.path.basename(p) not in EXCLUDE]
        lines = 0
        for p in paths:
            with open(p, encoding="utf-8", errors="replace") as f:
                lines += sum(1 for _ in f)
        out[name] = (lines, len(paths))

    scen = 0
    import glob
    for p in glob.glob(os.path.join(ROOT, "harness/scenarios/test_*.py")):
        scen += len(re.findall(r"^def test_", open(p).read(), re.M))
    out["#scenarios"] = (scen, 0)
    return out


def derive(m):
    """The figures the prose quotes, computed from the components."""
    spec = m["cascadia.py"][0]
    main = m["main_cascadia.c"][0]
    shared = sum(m[k][0] for k in
                 ("runtime/", "gbforge/", "harness/", "scripts/", "tests/"))
    return {
        "spec": spec,
        "main": main,
        "per_game": spec + main,
        "shared_round": round(shared / 100) * 100,
        "main_round": round(main / 10) * 10,
        "runtime_round": round(m["runtime/"][0] / 100) * 100,
        "scenarios": m["#scenarios"][0],
    }


# ── rewriting ────────────────────────────────────────────────────────

def fmt(n):
    return f"{n:,}"


def patch(text, m, d):
    """Return (text, [descriptions of what changed])."""
    changed = []

    # Table cells: match the row by its leading `path`, then rewrite the
    # digits in the size column without disturbing anything else.
    for name, (lines, files) in m.items():
        if name.startswith("#"):
            continue
        row = re.compile(r"^(\| `" + re.escape(name) + r"`[^|]*\| )([^|]*)(\|)",
                         re.M)
        mo = row.search(text)
        if not mo:
            changed.append(f"  ! no README row for {name}")
            continue
        cell = mo.group(2)
        want = cell
        nums = re.findall(r"\d[\d,]*", cell)
        if nums:
            want = want.replace(nums[0], fmt(lines), 1)
        if len(nums) > 1:
            head, sep, tail = want.partition(fmt(lines))
            want = head + sep + tail.replace(nums[1], str(files), 1)
        if want != cell:
            text = text[:mo.start(2)] + want + text[mo.end(2):]
            changed.append(f"  {name}: {cell.strip()} -> {want.strip()}")

    # Sentences. Each is anchored on words either side of the number so
    # a match can only be the claim it was written for.
    prose = [
        (r"(\*\*)[\d,]+( authored lines per game\.\*\*)", fmt(d["per_game"])),
        (r"(those )[\d,]+(\. The ratio is the argument)", fmt(d["per_game"])),
        (r"(Roughly )[\d,]+( lines of runtime, model, harness and gates)",
         fmt(d["shared_round"])),
        (r"(per-game C entry point\*\* \()[\d,]+( lines)", fmt(d["main"])),
        (r"(A second game costs )[\d,]+( lines of spec)", fmt(d["spec"])),
        (r"(~)[\d,]+(-line loop instead of a )", fmt(d["main_round"])),
        (r"(-line loop instead of a )[\d,]+(-line engine)",
         fmt(d["runtime_round"])),
        (r"(\[)[\d,]+( lines of Python\]\(examples/cascadia/cascadia\.py\))",
         fmt(d["spec"])),
        (r"(# )\d+( scenarios,)", str(d["scenarios"])),
    ]
    for pat, val in prose:
        rx = re.compile(pat)
        mo = rx.search(text)
        if not mo:
            changed.append(f"  ! no README match for /{pat}/")
            continue
        old = re.search(r"[\d,]+", mo.group(0)[len(mo.group(1)):]).group(0)
        if old != val:
            text = rx.sub(lambda x: x.group(1) + val + x.group(2), text, 1)
            changed.append(f"  prose: {old} -> {val}")
    return text, changed


def main(argv):
    write = "--write" in argv
    m = measure()
    d = derive(m)

    print("component                  lines  files")
    for name, (lines, files) in m.items():
        if name.startswith("#"):
            continue
        print(f"  {name:<24} {lines:>6} {files:>6}")
    print(f"  {'scenarios':<24} {d['scenarios']:>6}")
    print(f"\n  per game: {d['per_game']}    shared: "
          f"{sum(m[k][0] for k in ('runtime/','gbforge/','harness/','scripts/','tests/'))}")

    before = open(README, encoding="utf-8").read()
    after, changed = patch(before, m, d)
    problems = [c for c in changed if c.lstrip().startswith("!")]
    drift = [c for c in changed if not c.lstrip().startswith("!")]

    if problems:
        print("\nREADME structure changed — this script needs updating:")
        print("\n".join(problems))
        return 2

    if not drift:
        print("\ncheck_counts: README matches the tree")
        return 0

    if write:
        open(README, "w", encoding="utf-8").write(after)
        print("\ncheck_counts: updated README.md")
        print("\n".join(drift))
        return 0

    print("\ncheck_counts: README disagrees with the tree")
    print("\n".join(drift))
    print("\nrun: python3 scripts/check_counts.py --write")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
