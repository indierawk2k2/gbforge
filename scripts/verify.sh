#!/bin/bash
# Full verification, in the order that fails fastest.
#
# Every gate here has caught something real; none of them is
# redundant with another:
#
#   1. codegen       spec -> generated/ must be a no-op diff
#   2. transcript    the C engine and the Python model must agree
#   3. build         both ROMs link, banks check out
#   4. scenarios     memory + pixel behaviour under emulation
#   5. asset boundary the art tools and the runtime still agree
#
# GBDK_HOME must point at a GBDK-2020 4.5.0+ install (trailing slash).

set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# ── worktree support ────────────────────────────────────────────────
#
# Several agents working at once means several `git worktree` copies of
# this repo. The heavy dependencies are gitignored and NOT copied into
# a new worktree: SameBoy's source and built library (~1 min to fetch
# and build) and the symbol tables. Rebuilding them per worktree is
# pure waste, so link them from the main checkout instead.
#
# This is the difference between a second agent starting work in a
# second and a second agent starting work in two minutes.
GIT_COMMON="$(git rev-parse --git-common-dir 2>/dev/null || true)"
if [ -n "$GIT_COMMON" ] && [ "$GIT_COMMON" != ".git" ]; then
    MAIN_REPO="$(cd "$GIT_COMMON/.." && pwd)"
    for dep in harness/sameboy; do
        if [ ! -e "$ROOT/$dep" ] && [ -e "$MAIN_REPO/$dep" ]; then
            mkdir -p "$(dirname "$ROOT/$dep")"
            ln -s "$MAIN_REPO/$dep" "$ROOT/$dep"
            echo "verify: linked $dep -> $MAIN_REPO/$dep"
        fi
    done
fi

echo "=== 1/5 codegen is reproducible ==="
make -C examples/cascadia gen
if ! git diff --quiet -- examples/cascadia/generated; then
    echo "FAIL: generated/ differs from the spec — commit the regenerated output"
    git diff --stat -- examples/cascadia/generated
    exit 1
fi

echo "=== 2/5 engine transcript (C vs Python) ==="
make -C tests

echo "=== 3/5 ROM build ==="
make -C examples/cascadia
make -C examples/cascadia debug

echo "=== 4/5 emulator scenarios ==="
make -C harness test

echo "=== 5/5 asset boundary ==="
python3 -m pytest tests/test_asset_contracts.py -q

# The bank check runs inside the ROM link (see examples/cascadia/Makefile).
# It is the gate that matters most when several people or agents are
# adding code at once: ROM banks are the one genuinely shared, global
# resource in this codebase, and the linker reports an overflow by
# silently placing code at an address that is not code.

echo "=== all green ==="
