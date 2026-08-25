import Foundation

/// Holds 32x32 "cast" tiles for all 7 spell types — one per spell, no disabled variant.
/// These are displayed when a spell is cast in-game.
struct SpellCastTileSet: Equatable, Codable {
    /// Always exactly 7 tiles, indexed by SpellKind.rawValue.
    var tiles: [GBTile32x32]

    init() {
        tiles = (0..<7).map { _ in GBTile32x32() }
    }

    init(tiles: [GBTile32x32]) {
        precondition(tiles.count == 7)
        self.tiles = tiles
    }

    func tile(for spell: SpellKind) -> GBTile32x32 {
        tiles[spell.rawValue]
    }

    mutating func setTile(for spell: SpellKind, tile: GBTile32x32) {
        tiles[spell.rawValue] = tile
    }
}
