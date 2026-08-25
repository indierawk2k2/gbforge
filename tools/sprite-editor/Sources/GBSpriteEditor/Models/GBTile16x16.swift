import Foundation

/// A 16x16 Game Boy tile composed of 4 × 8x8 sub-tiles.
/// Layout: topLeft(+0), topRight(+1), bottomLeft(+2), bottomRight(+3)
/// All sub-tiles share a single palette.
struct GBTile16x16: Identifiable, Equatable, Codable {
    let id: UUID
    var name: String
    var subTiles: [GBTile8x8]  // [topLeft, topRight, bottomLeft, bottomRight]
    var paletteIndex: Int      // Single palette for the whole 16x16 tile

    init(name: String = "Untitled", paletteIndex: Int = 0) {
        self.id = UUID()
        self.name = name
        self.subTiles = [GBTile8x8(), GBTile8x8(), GBTile8x8(), GBTile8x8()]
        self.paletteIndex = paletteIndex
    }

    init(id: UUID = UUID(), name: String, subTiles: [GBTile8x8], paletteIndex: Int) {
        assert(subTiles.count == 4)
        self.id = id
        self.name = name
        self.subTiles = subTiles
        self.paletteIndex = paletteIndex
    }

    /// Quadrant index for a 16x16 pixel coordinate.
    static func quadrant(x: Int, y: Int) -> Int {
        (y < 8 ? 0 : 2) + (x < 8 ? 0 : 1)
    }

    /// Get pixel value (0-3) at a 16x16 coordinate.
    func pixel(x: Int, y: Int) -> UInt8 {
        let sub = Self.quadrant(x: x, y: y)
        return subTiles[sub].pixels[y % 8][x % 8]
    }

    /// Set pixel value (0-3) at a 16x16 coordinate.
    mutating func setPixel(x: Int, y: Int, value: UInt8) {
        let sub = Self.quadrant(x: x, y: y)
        subTiles[sub].pixels[y % 8][x % 8] = value
    }

    /// Encode all 4 sub-tiles to 64 bytes in GB order (TL, TR, BL, BR).
    func encode() -> [UInt8] {
        subTiles.flatMap { $0.encode() }
    }

    /// Decode from 64 bytes (4 × 16-byte sub-tiles in GB order).
    init(name: String, bytes: [UInt8], paletteIndex: Int) {
        assert(bytes.count == 64)
        self.id = UUID()
        self.name = name
        self.paletteIndex = paletteIndex
        self.subTiles = (0..<4).map { i in
            GBTile8x8(bytes: Array(bytes[i*16..<(i+1)*16]))
        }
    }
}
