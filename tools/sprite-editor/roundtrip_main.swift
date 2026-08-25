// Headless round-trip check — NOT part of the app target. Compiled
// standalone by scripts/editor-roundtrip.sh, so it runs on CI with
// no window server:
//   import res -> export tmp1 -> import tmp1 -> export tmp2
//   tmp1 and tmp2 must be byte-identical for every exported file.
//
// A drift here means the exporter emits something its own importer
// reads back differently. That failure is invisible from the game's
// side — the ROM still builds and still runs — and it surfaces as an
// artist opening the editor one day and finding their work subtly
// wrong. It belongs in CI, not in a bug report.

import Foundation

let exported = ["tiles_data.c", "palettes.c", "tiles_gfx.c",
                "spell_icons.c", "indicator_tiles.c"]

func fail(_ msg: String) -> Never {
    print("sprite-editor roundtrip: FAIL — \(msg)")
    exit(1)
}

func copyRes(_ src: URL, _ dst: URL) throws {
    try FileManager.default.createDirectory(at: dst,
                                            withIntermediateDirectories: true)
    for f in try FileManager.default.contentsOfDirectory(atPath: src.path) {
        if f.hasSuffix(".c") || f.hasSuffix(".h") {
            try FileManager.default.copyItem(
                at: src.appendingPathComponent(f),
                to: dst.appendingPathComponent(f))
        }
    }
}

guard CommandLine.arguments.count > 1 else {
    fail("usage: roundtrip <path-to-game/res>")
}
let res = URL(fileURLWithPath: CommandLine.arguments[1])
let tmpRoot = FileManager.default.temporaryDirectory
    .appendingPathComponent("sprite-rt-\(ProcessInfo.processInfo.processIdentifier)")
let tmp1 = tmpRoot.appendingPathComponent("gen1")
let tmp2 = tmpRoot.appendingPathComponent("gen2")

do {
    try copyRes(res, tmp1)
    try copyRes(res, tmp2)

    let (pal1, tiles1, icons1, ind1, cast1) =
        try CImporter.loadFromGameDirectory(res)
    if tiles1.isEmpty || pal1.isEmpty { fail("live res imported empty") }
    let doc1 = TileDocument()
    doc1.palettes = pal1
    doc1.tiles = tiles1
    doc1.spellIcons = icons1
    doc1.indicatorTiles = ind1
    doc1.spellCastTiles = cast1
    try CExporter.exportToGameDirectory(document: doc1, directory: tmp1)

    let (pal2, tiles2, icons2, ind2, cast2) =
        try CImporter.loadFromGameDirectory(tmp1)
    if tiles2.count != tiles1.count { fail("tile count drifted on re-import") }
    let doc2 = TileDocument()
    doc2.palettes = pal2
    doc2.tiles = tiles2
    doc2.spellIcons = icons2
    doc2.indicatorTiles = ind2
    doc2.spellCastTiles = cast2
    try CExporter.exportToGameDirectory(document: doc2, directory: tmp2)

    for f in exported {
        let a = tmp1.appendingPathComponent(f)
        let b = tmp2.appendingPathComponent(f)
        let da = try? Data(contentsOf: a)
        let db = try? Data(contentsOf: b)
        if da == nil && db == nil { continue }   // file not emitted
        if da != db { fail("\(f) not stable across re-import") }
    }
    try? FileManager.default.removeItem(at: tmpRoot)
    print("sprite-editor roundtrip: ok "
          + "(\(tiles1.count) tiles, \(pal1.count) palettes)")
} catch {
    fail("\(error)")
}
