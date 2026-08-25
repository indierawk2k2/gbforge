import Foundation

/// A 32x32 Game Boy tile stored as 16 sub-tiles (4x4 grid of 8x8 tiles) in row-major order.
/// Encodes to 256 bytes (16 x 16 bytes per sub-tile) in 2bpp format.
struct GBTile32x32: Equatable, Codable {
    /// 16 sub-tiles in row-major order: [row0col0, row0col1, row0col2, row0col3, row1col0, ...].
    var subTiles: [GBTile8x8]

    init() {
        subTiles = (0..<16).map { _ in GBTile8x8() }
    }

    init(subTiles: [GBTile8x8]) {
        precondition(subTiles.count == 16)
        self.subTiles = subTiles
    }

    /// Decode from 256-byte 2bpp data (16 consecutive 8x8 sub-tiles, row-major).
    init(bytes: [UInt8]) {
        precondition(bytes.count == 256)
        subTiles = (0..<16).map { i in
            GBTile8x8(bytes: Array(bytes[i * 16..<(i + 1) * 16]))
        }
    }

    /// Sub-tile index for a pixel coordinate (0-31, 0-31).
    private static func subTileIndex(x: Int, y: Int) -> Int {
        (y / 8) * 4 + (x / 8)
    }

    /// Read a pixel value (0-3) at the given coordinate.
    func pixel(x: Int, y: Int) -> UInt8 {
        subTiles[Self.subTileIndex(x: x, y: y)].pixels[y % 8][x % 8]
    }

    /// Write a pixel value (0-3) at the given coordinate.
    mutating func setPixel(x: Int, y: Int, value: UInt8) {
        subTiles[Self.subTileIndex(x: x, y: y)].pixels[y % 8][x % 8] = value
    }

    /// Encode to 256-byte 2bpp data (16 consecutive sub-tiles).
    func encode() -> [UInt8] {
        subTiles.flatMap { $0.encode() }
    }
}
