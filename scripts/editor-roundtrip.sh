#!/bin/bash
# Editor round-trip gate.
#
# The editor must import the live res pack, re-export it, re-import
# its own output, and export again byte-identically. Compiles the
# editor's UI-free Models + Services standalone with swiftc — the
# .app target is never launched, so this runs headless in CI.
#
# The failure this exists to catch: an exporter change that its own
# importer reads back differently. The ROM still builds, the game
# still runs, and the damage only appears the next time a human opens
# the tool. Nothing on the game side can see it.
#
# Usage: editor-roundtrip.sh [<res-dir>]

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RES="${1:-$ROOT/examples/cascadia/res}"
BUILD="${TMPDIR:-/tmp}/gbforge-editor-roundtrip"
mkdir -p "$BUILD"

SRC="$ROOT/tools/sprite-editor/Sources/GBSpriteEditor"
BIN="$BUILD/sprite-roundtrip"

# top-level code must live in a file literally named main.swift
cp "$ROOT/tools/sprite-editor/roundtrip_main.swift" "$BUILD/main.swift"
swiftc -O -o "$BIN" \
    "$SRC"/Models/*.swift \
    "$SRC"/Services/*.swift \
    "$BUILD/main.swift"

"$BIN" "$RES"
