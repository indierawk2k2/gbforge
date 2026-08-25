import Foundation

/// The four indicator tile types for 8x8 mini-icons used in quest mode HUD.
enum IndicatorKind: Int, CaseIterable, Codable {
    case emerald = 0, ruby = 1, obsidian = 2, aether = 3

    var name: String {
        switch self {
        case .emerald:  return "Emerald"
        case .ruby:     return "Ruby"
        case .obsidian: return "Obsidian"
        case .aether:   return "Aether"
        }
    }

    /// Palette index matching the full-size tile's palette assignment.
    var paletteIndex: Int {
        switch self {
        case .emerald:  return 2  // PAL_EARTH
        case .ruby:     return 0  // PAL_FIRE
        case .obsidian: return 7  // PAL_DARK
        case .aether:   return 1  // PAL_WATER
        }
    }
}

/// Holds pixel data for 4 indicator tiles (8x8 each): Emerald, Ruby, Obsidian, Aether.
struct IndicatorTileSet: Equatable, Codable {
    var tiles: [GBTile8x8]  // Always exactly 4

    init() {
        tiles = (0..<4).map { _ in GBTile8x8() }
    }

    init(tiles: [GBTile8x8]) {
        precondition(tiles.count == 4)
        self.tiles = tiles
    }
}
