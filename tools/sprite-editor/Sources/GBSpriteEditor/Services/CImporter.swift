import Foundation

/// Imports tile and palette data by parsing C source files (tiles_data.c, palettes.c)
/// from a game resource directory.
enum CImporter {

    enum ImportError: Error, LocalizedError {
        case fileNotFound(String)
        case parseFailed(String)

        var errorDescription: String? {
            switch self {
            case .fileNotFound(let name): return "File not found: \(name)"
            case .parseFailed(let msg): return "Parse error: \(msg)"
            }
        }
    }

    /// Load tiles, palettes, and spell icons from C source files in the given directory.
    /// tile_data[] may live in tiles_data.c or tiles_gfx.c (split out for ROM banking).
    static func loadFromGameDirectory(_ directory: URL) throws -> (palettes: [GBPalette], tiles: [GBTile16x16], spellIcons: SpellIconSet, indicatorTiles: IndicatorTileSet, spellCastTiles: SpellCastTileSet) {
        let tilesURL = directory.appendingPathComponent("tiles_data.c")
        let palettesURL = directory.appendingPathComponent("palettes.c")

        guard FileManager.default.fileExists(atPath: tilesURL.path) else {
            throw ImportError.fileNotFound("tiles_data.c")
        }
        guard FileManager.default.fileExists(atPath: palettesURL.path) else {
            throw ImportError.fileNotFound("palettes.c")
        }

        let tilesSource = try String(contentsOf: tilesURL, encoding: .utf8)
        let palettesSource = try String(contentsOf: palettesURL, encoding: .utf8)

        // tile_data[] may be in tiles_data.c or split into tiles_gfx.c for ROM banking
        let tileDataSource: String
        if tilesSource.contains("tile_data[]") {
            tileDataSource = tilesSource
        } else {
            let gfxURL = directory.appendingPathComponent("tiles_gfx.c")
            guard FileManager.default.fileExists(atPath: gfxURL.path) else {
                throw ImportError.parseFailed("tile_data[] not found in tiles_data.c or tiles_gfx.c")
            }
            tileDataSource = try String(contentsOf: gfxURL, encoding: .utf8)
        }

        // tile_palette_map names its palettes via palettes.h constants, so the
        // header has to be read before the map can be resolved.
        let constants = PaletteConstants.load(fromGameDirectory: directory) ?? .empty

        let palettes = try parsePalettes(from: palettesSource)
        let tileData = try parseTileData(from: tileDataSource)
        let paletteMap = try parsePaletteMap(from: tilesSource, constants: constants)
        let tileNames = parseTileNames(from: tileDataSource)

        guard tileData.count % 64 == 0 else {
            throw ImportError.parseFailed("Tile data size (\(tileData.count)) not a multiple of 64")
        }
        let tileCount = tileData.count / 64

        var tiles = [GBTile16x16]()
        for i in 0..<tileCount {
            let bytes = Array(tileData[i * 64 ..< (i + 1) * 64])
            let name = i < tileNames.count ? tileNames[i] : "Tile \(i)"
            let palIdx = i < paletteMap.count ? paletteMap[i] : 0
            let clampedIdx = max(0, min(palIdx, palettes.count - 1))
            tiles.append(GBTile16x16(name: name, bytes: bytes, paletteIndex: clampedIdx))
        }

        // Load spell icons (optional — fall back to defaults if missing or malformed)
        var spellIcons = SpellIconSet()
        let spellIconsURL = directory.appendingPathComponent("spell_icons.c")
        if FileManager.default.fileExists(atPath: spellIconsURL.path) {
            do {
                let spellSource = try String(contentsOf: spellIconsURL, encoding: .utf8)
                spellIcons = try parseSpellIcons(from: spellSource)
            } catch {
                print("Warning: failed to parse spell_icons.c, using defaults: \(error)")
            }
        }

        // Load indicator tiles (optional — fall back to defaults if missing or malformed)
        var indicatorTiles = IndicatorTileSet()
        let indicatorURL = directory.appendingPathComponent("indicator_tiles.c")
        if FileManager.default.fileExists(atPath: indicatorURL.path) {
            do {
                let indicatorSource = try String(contentsOf: indicatorURL, encoding: .utf8)
                indicatorTiles = try parseIndicatorTiles(from: indicatorSource)
            } catch {
                print("Warning: failed to parse indicator_tiles.c, using defaults: \(error)")
            }
        }

        // Load spell cast tiles (optional — fall back to defaults if missing)
        var spellCastTiles = SpellCastTileSet()
        if FileManager.default.fileExists(atPath: spellIconsURL.path) {
            do {
                let spellSource = try String(contentsOf: spellIconsURL, encoding: .utf8)
                if let parsed = try parseSpellCastTiles(from: spellSource) {
                    spellCastTiles = parsed
                }
            } catch {
                print("Warning: failed to parse spell_cast_tile_data, using defaults: \(error)")
            }
        }

        return (palettes, tiles, spellIcons, indicatorTiles, spellCastTiles)
    }

    // MARK: - Parsing

    private static func parsePalettes(from source: String) throws -> [GBPalette] {
        // Find bg_palettes array opening brace
        guard let headerRange = source.range(of: "bg_palettes", options: .literal) else {
            throw ImportError.parseFailed("Could not find bg_palettes array")
        }

        // Find the opening brace after bg_palettes
        guard let braceStart = source[headerRange.upperBound...].firstIndex(of: "{") else {
            throw ImportError.parseFailed("Could not find bg_palettes opening brace")
        }
        let contentStart = source.index(after: braceStart)

        // Find matching closing brace
        guard let braceEnd = findClosingBrace(in: source, from: contentStart) else {
            throw ImportError.parseFailed("Could not find bg_palettes closing brace")
        }

        let content = String(source[contentStart..<braceEnd])

        // Extract RGB(r, g, b) calls
        let rgbPattern = try NSRegularExpression(pattern: "RGB\\s*\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*\\)")
        let nsContent = content as NSString
        let matches = rgbPattern.matches(in: content, range: NSRange(location: 0, length: nsContent.length))

        guard matches.count >= 4, matches.count % 4 == 0 else {
            throw ImportError.parseFailed("Expected multiple of 4 RGB values, got \(matches.count)")
        }

        // Extract palette names from "// Name (N)" comments. The index inside the
        // parens is authoritative — names are NOT positional, because a comment may
        // carry trailing notes ("// Silver (7 — was 4; ...)") or be missing entirely,
        // which would otherwise shift every later name onto the wrong palette.
        let namePattern = try NSRegularExpression(pattern: "//\\s*(.+?)\\s*\\((\\d+)")
        let nameMatches = namePattern.matches(in: content, range: NSRange(location: 0, length: nsContent.length))

        var namesByIndex = [Int: String]()
        for match in nameMatches {
            guard let index = Int(nsContent.substring(with: match.range(at: 2))) else { continue }
            namesByIndex[index] = nsContent.substring(with: match.range(at: 1))
                .trimmingCharacters(in: .whitespaces)
        }

        var palettes = [GBPalette]()
        let paletteCount = matches.count / 4

        for i in 0..<paletteCount {
            var colors = [GBColor]()
            for j in 0..<4 {
                let m = matches[i * 4 + j]
                let r = UInt8(nsContent.substring(with: m.range(at: 1)))!
                let g = UInt8(nsContent.substring(with: m.range(at: 2)))!
                let b = UInt8(nsContent.substring(with: m.range(at: 3)))!
                colors.append(GBColor(r: r, g: g, b: b))
            }
            palettes.append(GBPalette(name: namesByIndex[i] ?? "Palette \(i)", colors: colors))
        }

        return palettes
    }

    private static func parseTileData(from source: String) throws -> [UInt8] {
        // Find tile_data[] array
        guard let headerRange = source.range(of: "tile_data[]", options: .literal) else {
            throw ImportError.parseFailed("Could not find tile_data array")
        }

        guard let braceStart = source[headerRange.upperBound...].firstIndex(of: "{") else {
            throw ImportError.parseFailed("Could not find tile_data opening brace")
        }
        let contentStart = source.index(after: braceStart)

        guard let braceEnd = findClosingBrace(in: source, from: contentStart) else {
            throw ImportError.parseFailed("Could not find tile_data closing brace")
        }

        let content = String(source[contentStart..<braceEnd])

        // Extract hex bytes
        let hexPattern = try NSRegularExpression(pattern: "0[xX]([0-9A-Fa-f]{2})")
        let nsContent = content as NSString
        let matches = hexPattern.matches(in: content, range: NSRange(location: 0, length: nsContent.length))

        return matches.map { match in
            UInt8(nsContent.substring(with: match.range(at: 1)), radix: 16)!
        }
    }

    private static func parsePaletteMap(from source: String, constants: PaletteConstants) throws -> [Int] {
        // Find tile_palette_map array
        guard let headerRange = source.range(of: "tile_palette_map", options: .literal) else {
            throw ImportError.parseFailed("Could not find tile_palette_map array")
        }

        guard let braceStart = source[headerRange.upperBound...].firstIndex(of: "{") else {
            throw ImportError.parseFailed("Could not find tile_palette_map opening brace")
        }
        let contentStart = source.index(after: braceStart)

        guard let braceEnd = findClosingBrace(in: source, from: contentStart) else {
            throw ImportError.parseFailed("Could not find tile_palette_map closing brace")
        }

        // Comments carry palette-ish words ("// Silver") that would otherwise
        // read as entries once identifiers are accepted below.
        let content = stripComments(String(source[contentStart..<braceEnd]))
        let nsContent = content as NSString
        let fullRange = NSRange(location: 0, length: nsContent.length)

        // An entry is either a literal index or a palettes.h constant (PAL_SILVER, ...)
        let entry = "([A-Za-z_][A-Za-z0-9_]*|\\d+)"

        // Try [][4] format first: { E, E, E, E } — one row per tile, first column wins
        let rowPattern = try NSRegularExpression(pattern: "\\{\\s*" + entry)
        let rowMatches = rowPattern.matches(in: content, range: fullRange)

        if !rowMatches.isEmpty {
            return try rowMatches.map { match in
                try resolve(nsContent.substring(with: match.range(at: 1)), constants)
            }
        }

        // Try flat format: E, E, E, ...
        let flatPattern = try NSRegularExpression(pattern: entry + "\\s*,")
        let flatMatches = flatPattern.matches(in: content, range: fullRange)

        guard !flatMatches.isEmpty else {
            throw ImportError.parseFailed("tile_palette_map has no recognisable entries")
        }

        return try flatMatches.map { match in
            try resolve(nsContent.substring(with: match.range(at: 1)), constants)
        }
    }

    /// Resolve one palette map entry, failing loudly rather than defaulting to 0 —
    /// a silent fallback here is what makes every tile render on palette 0.
    private static func resolve(_ token: String, _ constants: PaletteConstants) throws -> Int {
        guard let value = constants.resolve(token) else {
            throw ImportError.parseFailed("tile_palette_map uses \(token), which palettes.h does not define")
        }
        return value
    }

    private static func parseTileNames(from source: String) -> [String] {
        // Extract tile names from "// === Name (N) ===" comments
        let pattern = try! NSRegularExpression(pattern: "//\\s*===\\s*(.+?)\\s*\\(\\d+\\)\\s*===")
        let nsSource = source as NSString
        let matches = pattern.matches(in: source, range: NSRange(location: 0, length: nsSource.length))

        return matches.map { match in
            nsSource.substring(with: match.range(at: 1)).trimmingCharacters(in: .whitespaces)
        }
    }

    private static func parseSpellIcons(from source: String) throws -> SpellIconSet {
        guard let headerRange = source.range(of: "spell_icon_tile_data[]", options: .literal) else {
            throw ImportError.parseFailed("Could not find spell_icon_tile_data array")
        }

        guard let braceStart = source[headerRange.upperBound...].firstIndex(of: "{") else {
            throw ImportError.parseFailed("Could not find spell_icon_tile_data opening brace")
        }
        let contentStart = source.index(after: braceStart)

        guard let braceEnd = findClosingBrace(in: source, from: contentStart) else {
            throw ImportError.parseFailed("Could not find spell_icon_tile_data closing brace")
        }

        let content = String(source[contentStart..<braceEnd])

        let hexPattern = try NSRegularExpression(pattern: "0[xX]([0-9A-Fa-f]{2})")
        let nsContent = content as NSString
        let matches = hexPattern.matches(in: content, range: NSRange(location: 0, length: nsContent.length))

        let bytes = matches.map { match in
            UInt8(nsContent.substring(with: match.range(at: 1)), radix: 16)!
        }

        let icons: [GBTile8x8]
        if bytes.count == 224 {
            // New 7-spell format: 14 tiles
            icons = (0..<14).map { i in
                GBTile8x8(bytes: Array(bytes[i * 16..<(i + 1) * 16]))
            }
        } else if bytes.count == 192 {
            // Legacy 6-spell format: 12 tiles — migrate to 14
            let oldIcons = (0..<12).map { i in
                GBTile8x8(bytes: Array(bytes[i * 16..<(i + 1) * 16]))
            }
            icons = SpellIconSet.migrateFrom6Spells(oldIcons)
        } else {
            throw ImportError.parseFailed("Expected 192 or 224 spell icon bytes, got \(bytes.count)")
        }
        return SpellIconSet(icons: icons)
    }

    private static func parseIndicatorTiles(from source: String) throws -> IndicatorTileSet {
        guard let headerRange = source.range(of: "indicator_tile_data[]", options: .literal) else {
            throw ImportError.parseFailed("Could not find indicator_tile_data array")
        }

        guard let braceStart = source[headerRange.upperBound...].firstIndex(of: "{") else {
            throw ImportError.parseFailed("Could not find indicator_tile_data opening brace")
        }
        let contentStart = source.index(after: braceStart)

        guard let braceEnd = findClosingBrace(in: source, from: contentStart) else {
            throw ImportError.parseFailed("Could not find indicator_tile_data closing brace")
        }

        let content = String(source[contentStart..<braceEnd])

        let hexPattern = try NSRegularExpression(pattern: "0[xX]([0-9A-Fa-f]{2})")
        let nsContent = content as NSString
        let matches = hexPattern.matches(in: content, range: NSRange(location: 0, length: nsContent.length))

        let bytes = matches.map { match in
            UInt8(nsContent.substring(with: match.range(at: 1)), radix: 16)!
        }

        guard bytes.count == 64 else {
            throw ImportError.parseFailed("Expected 64 indicator tile bytes, got \(bytes.count)")
        }

        let tiles = (0..<4).map { i in
            GBTile8x8(bytes: Array(bytes[i * 16..<(i + 1) * 16]))
        }
        return IndicatorTileSet(tiles: tiles)
    }

    private static func parseSpellCastTiles(from source: String) throws -> SpellCastTileSet? {
        guard let headerRange = source.range(of: "spell_cast_tile_data[]", options: .literal) else {
            return nil  // Not present in older files
        }

        guard let braceStart = source[headerRange.upperBound...].firstIndex(of: "{") else {
            throw ImportError.parseFailed("Could not find spell_cast_tile_data opening brace")
        }
        let contentStart = source.index(after: braceStart)

        guard let braceEnd = findClosingBrace(in: source, from: contentStart) else {
            throw ImportError.parseFailed("Could not find spell_cast_tile_data closing brace")
        }

        let content = String(source[contentStart..<braceEnd])

        let hexPattern = try NSRegularExpression(pattern: "0[xX]([0-9A-Fa-f]{2})")
        let nsContent = content as NSString
        let matches = hexPattern.matches(in: content, range: NSRange(location: 0, length: nsContent.length))

        let bytes = matches.map { match in
            UInt8(nsContent.substring(with: match.range(at: 1)), radix: 16)!
        }

        guard bytes.count == 1792 else {
            throw ImportError.parseFailed("Expected 1792 spell cast tile bytes, got \(bytes.count)")
        }

        let tiles = (0..<7).map { i in
            GBTile32x32(bytes: Array(bytes[i * 256..<(i + 1) * 256]))
        }
        return SpellCastTileSet(tiles: tiles)
    }

    // MARK: - Helpers

    /// Strip `//` and `/* */` comments from an array body.
    private static func stripComments(_ source: String) -> String {
        guard let pattern = try? NSRegularExpression(pattern: "//[^\n]*|/\\*.*?\\*/",
                                                    options: .dotMatchesLineSeparators) else {
            return source
        }
        let ns = source as NSString
        return pattern.stringByReplacingMatches(in: source,
                                                range: NSRange(location: 0, length: ns.length),
                                                withTemplate: "")
    }

    /// Find the matching closing brace, accounting for nested braces.
    private static func findClosingBrace(in source: String, from start: String.Index) -> String.Index? {
        var depth = 1
        for idx in source[start...].indices {
            switch source[idx] {
            case "{": depth += 1
            case "}":
                depth -= 1
                if depth == 0 { return idx }
            default: break
            }
        }
        return nil
    }
}
