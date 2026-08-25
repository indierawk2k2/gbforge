import Foundation

/// The seven spell types, ordered to match SPELL_SPARK=1 through SPELL_ONEUP=7 in spells.h.
enum SpellKind: Int, CaseIterable, Codable {
    case spark    = 0
    case gaia     = 1
    case aqua     = 2
    case tectonic = 3
    case slide    = 4
    case dust     = 5
    case oneup    = 6

    /// Palette index matching spell_to_pal() in render.c.
    var paletteIndex: Int {
        switch self {
        case .spark, .dust:              return 0  // PAL_FIRE
        case .aqua, .slide:              return 1  // PAL_WATER
        case .gaia, .tectonic, .oneup:   return 2  // PAL_EARTH
        }
    }
}

/// Holds pixel data for all 7 spell icons, each with normal and disabled variants (14 total 8x8 tiles).
/// Storage order: indices 0-6 = normal icons (spark..oneup), indices 7-13 = disabled variants.
struct SpellIconSet: Equatable, Codable {
    var icons: [GBTile8x8]

    /// Default tile data (14 tiles x 16 bytes = 224 bytes).
    /// Order: Spark, Gaia, Aqua, Tectonic, Slide, Dust, OneUp, then 7 disabled variants.
    /// Tectonic duplicates Gaia's art as placeholder.
    private static let defaultTileData: [UInt8] = [
        // Spark icon
        0x0C,0x0C, 0x18,0x18, 0x30,0x30, 0x7E,0x7E,
        0x1C,0x1C, 0x18,0x18, 0x30,0x30, 0x60,0x60,
        // Gaia icon (was Skin)
        0x00,0x00, 0x7E,0x7E, 0x7E,0x42, 0x7E,0x42,
        0x7E,0x42, 0x3C,0x24, 0x18,0x18, 0x00,0x00,
        // Aqua icon (was Midas)
        0x00,0x00, 0x42,0x42, 0x66,0x66, 0x5A,0x5A,
        0x42,0x42, 0x42,0x42, 0x7E,0x7E, 0x00,0x00,
        // Tectonic icon (placeholder: duplicates Gaia)
        0x00,0x00, 0x7E,0x7E, 0x7E,0x42, 0x7E,0x42,
        0x7E,0x42, 0x3C,0x24, 0x18,0x18, 0x00,0x00,
        // Slide icon
        0x00,0x00, 0x24,0x24, 0x66,0x66, 0xFF,0xFF,
        0xFF,0xFF, 0x66,0x66, 0x24,0x24, 0x00,0x00,
        // Dust icon
        0x00,0x00, 0x1C,0x1C, 0x36,0x22, 0x7F,0x41,
        0x7F,0x41, 0x36,0x22, 0x1C,0x1C, 0x00,0x00,
        // 1Up mushroom icon
        0x38,0x38, 0x44,0x7C, 0x99,0xE7, 0x81,0xFF,
        0x7E,0x7E, 0x24,0x3C, 0x24,0x3C, 0x18,0x18,
        // Spark disabled
        0x00,0x0C, 0x00,0x18, 0x00,0x30, 0x00,0x7E,
        0x00,0x1C, 0x00,0x18, 0x00,0x30, 0x00,0x60,
        // Gaia disabled (was Skin disabled)
        0x00,0x00, 0x00,0x7E, 0x3C,0x42, 0x3C,0x42,
        0x3C,0x42, 0x18,0x24, 0x00,0x18, 0x00,0x00,
        // Aqua disabled (was Midas disabled)
        0x00,0x00, 0x00,0x42, 0x00,0x66, 0x00,0x5A,
        0x00,0x42, 0x00,0x42, 0x00,0x7E, 0x00,0x00,
        // Tectonic disabled (placeholder: duplicates Gaia disabled)
        0x00,0x00, 0x00,0x7E, 0x3C,0x42, 0x3C,0x42,
        0x3C,0x42, 0x18,0x24, 0x00,0x18, 0x00,0x00,
        // Slide disabled
        0x00,0x00, 0x00,0x24, 0x00,0x66, 0x00,0xFF,
        0x00,0xFF, 0x00,0x66, 0x00,0x24, 0x00,0x00,
        // Dust disabled
        0x00,0x00, 0x00,0x1C, 0x14,0x22, 0x3E,0x41,
        0x3E,0x41, 0x14,0x22, 0x00,0x1C, 0x00,0x00,
        // 1Up mushroom disabled
        0x00,0x38, 0x00,0x7C, 0x18,0xE7, 0x00,0xFF,
        0x00,0x7E, 0x00,0x3C, 0x00,0x3C, 0x00,0x18,
    ]

    init() {
        icons = (0..<14).map { i in
            let offset = i * 16
            return GBTile8x8(bytes: Array(Self.defaultTileData[offset..<offset + 16]))
        }
    }

    init(icons: [GBTile8x8]) {
        self.icons = icons
    }

    func icon(for spell: SpellKind, disabled: Bool) -> GBTile8x8 {
        icons[spell.rawValue + (disabled ? 7 : 0)]
    }

    mutating func setIcon(for spell: SpellKind, disabled: Bool, tile: GBTile8x8) {
        icons[spell.rawValue + (disabled ? 7 : 0)] = tile
    }

    /// Migrate from the old 6-spell / 12-tile layout to the new 7-spell / 14-tile layout.
    /// Old order: [Spark, Dust, Slide, Skin, Midas, 1Up] + 6 disabled
    /// New order: [Spark, Gaia, Aqua, Tectonic, Slide, Dust, OneUp] + 7 disabled
    static func migrateFrom6Spells(_ oldIcons: [GBTile8x8]) -> [GBTile8x8] {
        precondition(oldIcons.count == 12)
        // Old indices: 0=Spark, 1=Dust, 2=Slide, 3=Skin(→Gaia), 4=Midas(→Aqua), 5=1Up
        // Old disabled: 6=Spark, 7=Dust, 8=Slide, 9=Skin(→Gaia), 10=Midas(→Aqua), 11=1Up
        var newIcons = [GBTile8x8]()
        // Normal icons (7)
        newIcons.append(oldIcons[0])  // Spark
        newIcons.append(oldIcons[3])  // Gaia (was Skin)
        newIcons.append(oldIcons[4])  // Aqua (was Midas)
        newIcons.append(oldIcons[3])  // Tectonic (copy of Gaia/Skin as placeholder)
        newIcons.append(oldIcons[2])  // Slide
        newIcons.append(oldIcons[1])  // Dust
        newIcons.append(oldIcons[5])  // OneUp
        // Disabled icons (7)
        newIcons.append(oldIcons[6])  // Spark disabled
        newIcons.append(oldIcons[9])  // Gaia disabled (was Skin disabled)
        newIcons.append(oldIcons[10]) // Aqua disabled (was Midas disabled)
        newIcons.append(oldIcons[9])  // Tectonic disabled (copy of Gaia/Skin disabled)
        newIcons.append(oldIcons[8])  // Slide disabled
        newIcons.append(oldIcons[7])  // Dust disabled
        newIcons.append(oldIcons[11]) // OneUp disabled
        return newIcons
    }
}
