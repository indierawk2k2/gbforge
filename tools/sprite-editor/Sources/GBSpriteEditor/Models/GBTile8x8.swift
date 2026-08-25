import Foundation

/// An 8x8 Game Boy tile stored as 2bpp pixel data.
struct GBTile8x8: Equatable, Codable {
    /// 8 rows of 8 pixels, each pixel is 0-3.
    var pixels: [[UInt8]]  // [row][col], row 0 = top

    init() {
        pixels = Array(repeating: Array(repeating: 0, count: 8), count: 8)
    }

    init(pixels: [[UInt8]]) {
        assert(pixels.count == 8 && pixels.allSatisfy { $0.count == 8 })
        self.pixels = pixels
    }

    /// Decode from 16-byte 2bpp data (Game Boy native format).
    /// Each row = 2 bytes: lo byte (bit 0 of each pixel), hi byte (bit 1).
    /// Bit 7 = leftmost pixel.
    init(bytes: [UInt8]) {
        assert(bytes.count == 16)
        pixels = Array(repeating: Array(repeating: UInt8(0), count: 8), count: 8)
        for row in 0..<8 {
            let lo = bytes[row * 2]
            let hi = bytes[row * 2 + 1]
            for col in 0..<8 {
                let bit = 7 - col  // bit 7 = leftmost pixel
                let loBit = (lo >> bit) & 1
                let hiBit = (hi >> bit) & 1
                pixels[row][col] = (hiBit << 1) | loBit
            }
        }
    }

    /// Encode to 16-byte 2bpp data (Game Boy native format).
    func encode() -> [UInt8] {
        var bytes = [UInt8](repeating: 0, count: 16)
        for row in 0..<8 {
            var lo: UInt8 = 0
            var hi: UInt8 = 0
            for col in 0..<8 {
                let bit = 7 - col
                let p = pixels[row][col]
                lo |= ((p & 1) << bit)
                hi |= (((p >> 1) & 1) << bit)
            }
            bytes[row * 2] = lo
            bytes[row * 2 + 1] = hi
        }
        return bytes
    }
}
