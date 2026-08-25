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

echo "=== all green ==="
