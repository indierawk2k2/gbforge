import Foundation

/// Binary `.gbtiles` file format reader/writer.
///
/// Format (v6):
/// ```
/// Header (8 bytes):
///   [0-3]  Magic: "GBTL"
///   [4]    Version: 5 (uint8)
///   [5]    Palette count N (uint8)
///   [6]    Tile count M (uint8)
///   [7]    Reserved: 0
///
/// Palette Section (N × 24 bytes each):
///   [0-15]  Name (16 bytes, null-padded UTF-8)
///   [16-23] Colors: 4 × uint16 LE (RGB555)
///
/// Tile Section (M × 97 bytes each):
///   [0-31]   Name (32 bytes, null-padded UTF-8)
///   [32]     Palette index: 1 × uint8
///   [33-96]  Pixel data: 64 bytes (4 × 16-byte 2bpp sub-tiles)
///
/// Spell Icon Section:
///   v3: 12 × 16-byte GBTile8x8 (6 normal + 6 disabled) = 192 bytes
///   v4+: 14 × 16-byte GBTile8x8 (7 normal + 7 disabled) = 224 bytes
///
/// Indicator Tile Section (v5+):
///   4 × 16-byte GBTile8x8 (Emerald, Ruby, Obsidian, Aether) = 64 bytes
///
/// Spell Cast Tile Section (v6+):
///   7 × 256-byte GBTile32x32 (Spark..OneUp, 16 sub-tiles each) = 1792 bytes
/// ```
enum GBTilesFile {

    static let magic: [UInt8] = [0x47, 0x42, 0x54, 0x4C] // "GBTL"
    static let currentVersion: UInt8 = 6
    static let headerSize = 8
    static let paletteRecordSize = 24
    static let tileRecordSize = 97  // 32 name + 1 palette + 64 pixels
    static let spellIconSectionSize = 224    // 14 × 16 bytes (v4+: 7 spells)
    static let spellIconSectionSizeV3 = 192  // 12 × 16 bytes (v3: 6 spells)
    static let indicatorSectionSize = 64     // 4 × 16 bytes (v5+: 4 indicator tiles)
    static let spellCastSectionSize = 1792   // 7 × 256 bytes (v6+: 7 spell cast 32x32 tiles)

    enum FileError: Error, LocalizedError {
        case invalidMagic
        case unsupportedVersion(UInt8)
        case unexpectedFileSize
        case invalidData(String)

        var errorDescription: String? {
            switch self {
            case .invalidMagic: return "Not a valid .gbtiles file"
            case .unsupportedVersion(let v): return "Unsupported .gbtiles version \(v)"
            case .unexpectedFileSize: return "File size doesn't match header"
            case .invalidData(let msg): return "Invalid data: \(msg)"
            }
        }
    }

    // MARK: - Load

    static func load(from url: URL) throws -> (palettes: [GBPalette], tiles: [GBTile16x16], spellIcons: SpellIconSet, indicatorTiles: IndicatorTileSet, spellCastTiles: SpellCastTileSet) {
        let data = try Data(contentsOf: url)
        guard data.count >= headerSize else {
            throw FileError.unexpectedFileSize
        }

        // Validate magic
        let fileMagic = Array(data[0..<4])
        guard fileMagic == magic else {
            throw FileError.invalidMagic
        }

        // Read header
        let version = data[4]

        let paletteCount = Int(data[5])
        let tileCount = Int(data[6])
        // data[7] is reserved

        let tileSize: Int
        if version == 1 {
            tileSize = 100  // 32 name + 4 palette + 64 pixels
        } else if version >= 2 && version <= 6 {
            tileSize = 97   // 32 name + 1 palette + 64 pixels
        } else {
            throw FileError.unsupportedVersion(version)
        }

        var expectedSize = headerSize + paletteCount * paletteRecordSize + tileCount * tileSize
        if version >= 4 {
            expectedSize += spellIconSectionSize
        } else if version == 3 {
            expectedSize += spellIconSectionSizeV3
        }
        if version >= 5 {
            expectedSize += indicatorSectionSize
        }
        if version >= 6 {
            expectedSize += spellCastSectionSize
        }
        guard data.count == expectedSize else {
            throw FileError.unexpectedFileSize
        }

        // Read palettes
        var palettes = [GBPalette]()
        for i in 0..<paletteCount {
            let offset = headerSize + i * paletteRecordSize
            let name = readNullPaddedString(from: data, offset: offset, maxLength: 16)
            var colors = [GBColor]()
            for c in 0..<4 {
                let colorOffset = offset + 16 + c * 2
                let raw = UInt16(data[colorOffset]) | (UInt16(data[colorOffset + 1]) << 8)
                colors.append(GBColor(raw: raw))
            }
            palettes.append(GBPalette(name: name, colors: colors))
        }

        // Read tiles
        var tiles = [GBTile16x16]()
        let tileBaseOffset = headerSize + paletteCount * paletteRecordSize
        for i in 0..<tileCount {
            let offset = tileBaseOffset + i * tileSize
            let name = readNullPaddedString(from: data, offset: offset, maxLength: 32)
            let paletteIndex: Int
            let pixelDataStart: Int
            if version == 1 {
                // v1: take first of 4 palette indices
                paletteIndex = Int(data[offset + 32])
                pixelDataStart = offset + 36
            } else {
                paletteIndex = Int(data[offset + 32])
                pixelDataStart = offset + 33
            }
            let pixelData = Array(data[pixelDataStart..<(pixelDataStart + 64)])
            tiles.append(GBTile16x16(name: name, bytes: pixelData, paletteIndex: paletteIndex))
        }

        // Read spell icons (v3+), otherwise use defaults
        let spellIcons: SpellIconSet
        if version >= 4 {
            // v4+: 14 tiles (7 normal + 7 disabled)
            let spellDataOffset = tileBaseOffset + tileCount * tileSize
            var icons = [GBTile8x8]()
            for i in 0..<14 {
                let offset = spellDataOffset + i * 16
                let bytes = Array(data[offset..<(offset + 16)])
                icons.append(GBTile8x8(bytes: bytes))
            }
            spellIcons = SpellIconSet(icons: icons)
        } else if version == 3 {
            // v3: 12 tiles (6 normal + 6 disabled) — migrate to 14
            let spellDataOffset = tileBaseOffset + tileCount * tileSize
            var oldIcons = [GBTile8x8]()
            for i in 0..<12 {
                let offset = spellDataOffset + i * 16
                let bytes = Array(data[offset..<(offset + 16)])
                oldIcons.append(GBTile8x8(bytes: bytes))
            }
            spellIcons = SpellIconSet(icons: SpellIconSet.migrateFrom6Spells(oldIcons))
        } else {
            spellIcons = SpellIconSet()
        }

        // Read indicator tiles (v5+), otherwise use defaults
        let indicatorTiles: IndicatorTileSet
        if version >= 5 {
            let spellDataOffset = tileBaseOffset + tileCount * tileSize
            let indicatorDataOffset = spellDataOffset + spellIconSectionSize
            var tiles = [GBTile8x8]()
            for i in 0..<4 {
                let offset = indicatorDataOffset + i * 16
                let bytes = Array(data[offset..<(offset + 16)])
                tiles.append(GBTile8x8(bytes: bytes))
            }
            indicatorTiles = IndicatorTileSet(tiles: tiles)
        } else {
            indicatorTiles = IndicatorTileSet()
        }

        // Read spell cast tiles (v6+), otherwise use defaults
        let spellCastTiles: SpellCastTileSet
        if version >= 6 {
            let spellDataOffset = tileBaseOffset + tileCount * tileSize
            let castDataOffset = spellDataOffset + spellIconSectionSize + indicatorSectionSize
            var castTiles = [GBTile32x32]()
            for i in 0..<7 {
                let offset = castDataOffset + i * 256
                let bytes = Array(data[offset..<(offset + 256)])
                castTiles.append(GBTile32x32(bytes: bytes))
            }
            spellCastTiles = SpellCastTileSet(tiles: castTiles)
        } else {
            spellCastTiles = SpellCastTileSet()
        }

        return (palettes, tiles, spellIcons, indicatorTiles, spellCastTiles)
    }

    // MARK: - Save

    static func save(palettes: [GBPalette], tiles: [GBTile16x16], spellIcons: SpellIconSet, spellCastTiles: SpellCastTileSet = SpellCastTileSet(), indicatorTiles: IndicatorTileSet = IndicatorTileSet(), to url: URL) throws {
        guard palettes.count <= 255 else {
            throw FileError.invalidData("Too many palettes (\(palettes.count), max 255)")
        }
        guard tiles.count <= 255 else {
            throw FileError.invalidData("Too many tiles (\(tiles.count), max 255)")
        }

        let totalSize = headerSize + palettes.count * paletteRecordSize + tiles.count * tileRecordSize + spellIconSectionSize + indicatorSectionSize + spellCastSectionSize
        var data = Data(capacity: totalSize)

        // Header
        data.append(contentsOf: magic)
        data.append(currentVersion)
        data.append(UInt8(palettes.count))
        data.append(UInt8(tiles.count))
        data.append(0) // reserved

        // Palettes
        for palette in palettes {
            data.append(contentsOf: writeNullPaddedString(palette.name, length: 16))
            for color in palette.colors {
                let raw = color.raw
                data.append(UInt8(raw & 0xFF))
                data.append(UInt8((raw >> 8) & 0xFF))
            }
        }

        // Tiles
        for tile in tiles {
            data.append(contentsOf: writeNullPaddedString(tile.name, length: 32))
            data.append(UInt8(tile.paletteIndex))
            data.append(contentsOf: tile.encode())
        }

        // Spell icons (14 × 16 bytes)
        for icon in spellIcons.icons {
            data.append(contentsOf: icon.encode())
        }

        // Indicator tiles (4 × 16 bytes)
        for tile in indicatorTiles.tiles {
            data.append(contentsOf: tile.encode())
        }

        // Spell cast tiles (7 × 256 bytes)
        for castTile in spellCastTiles.tiles {
            data.append(contentsOf: castTile.encode())
        }

        assert(data.count == totalSize)
        try data.write(to: url)
    }

    // MARK: - String helpers

    private static func readNullPaddedString(from data: Data, offset: Int, maxLength: Int) -> String {
        let bytes = Array(data[offset..<(offset + maxLength)])
        if let nullIndex = bytes.firstIndex(of: 0) {
            return String(bytes: Array(bytes[0..<nullIndex]), encoding: .utf8) ?? "Untitled"
        }
        return String(bytes: bytes, encoding: .utf8) ?? "Untitled"
    }

    private static func writeNullPaddedString(_ string: String, length: Int) -> [UInt8] {
        var bytes = Array(string.utf8)
        if bytes.count > length {
            bytes = Array(bytes.prefix(length))
        }
        while bytes.count < length {
            bytes.append(0)
        }
        return bytes
    }
}
