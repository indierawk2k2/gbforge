import Foundation

/// A text sprite that composes characters from the pixel font into a sprite strip.
/// After compose(), the pixel grid is stored and becomes the source of truth for
/// rendering and export. Users can edit individual pixels on the canvas.
struct TextSprite: Identifiable, Equatable {
    let id: UUID
    var name: String
    var text: String
    /// Stored, editable pixel grid (source of truth for export + display).
    /// Values: 0=transparent, 1=white, 2=medium, 3=black.
    var pixels: [[UInt8]]

    init(id: UUID = UUID(), name: String = "Text", text: String = "CHAIN") {
        self.id = id
        self.name = name
        self.text = text
        self.pixels = [[UInt8]](repeating: [UInt8](repeating: 0, count: 8), count: 16)
    }

    static func == (lhs: TextSprite, rhs: TextSprite) -> Bool {
        lhs.id == rhs.id && lhs.name == rhs.name && lhs.text == rhs.text &&
        lhs.pixels == rhs.pixels
    }

    /// Compose the text into the stored pixel grid (height=16, width=multiple of 8).
    /// No transparent mode: ink -> 3 (black), non-ink -> 1 (white).
    /// Characters touch — no inter-character spacing.
    mutating func compose(font: PixelFont) {
        let upperText = text.uppercased()

        // Collect glyphs
        var charGlyphs: [PixelGlyph] = []
        for ch in upperText {
            if let glyph = font.glyphs[ch] {
                charGlyphs.append(glyph)
            }
        }

        guard !charGlyphs.isEmpty else {
            pixels = [[UInt8]](repeating: [UInt8](repeating: 1, count: 8), count: 16)
            return
        }

        // Calculate dimensions — no spacing, chars touch
        let totalWidth = charGlyphs.reduce(0) { $0 + $1.width }
        let paddedWidth = ((totalWidth + 7) / 8) * 8
        let maxGlyphHeight = charGlyphs.map(\.height).max() ?? 0
        let paddedHeight = 16

        // Start with all-white grid
        var grid = [[UInt8]](repeating: [UInt8](repeating: 1, count: paddedWidth), count: paddedHeight)
        let yOffset = max(0, (paddedHeight - maxGlyphHeight) / 2)

        // Stamp black ink pixels — no spacing between characters
        var xCursor = 0
        for glyph in charGlyphs {
            for dy in 0..<glyph.height {
                let destY = yOffset + dy
                guard destY < paddedHeight else { continue }
                for dx in 0..<glyph.width {
                    let destX = xCursor + dx
                    guard destX < paddedWidth else { continue }
                    if glyph.pixels[dy][dx] {
                        grid[destY][destX] = 3  // black
                    }
                }
            }
            xCursor += glyph.width
        }

        pixels = grid
    }

    /// Export as GBTile8x8 tiles: all top halves first, then all bottom halves.
    /// Reads from stored pixels.
    func exportTiles() -> [GBTile8x8] {
        let cols = columnCount

        var topTiles: [GBTile8x8] = []
        var bottomTiles: [GBTile8x8] = []

        for col in 0..<cols {
            var topPixels = [[UInt8]](repeating: [UInt8](repeating: 0, count: 8), count: 8)
            for row in 0..<8 {
                for px in 0..<8 {
                    topPixels[row][px] = pixels[row][col * 8 + px]
                }
            }
            topTiles.append(GBTile8x8(pixels: topPixels))

            var bottomPixels = [[UInt8]](repeating: [UInt8](repeating: 0, count: 8), count: 8)
            for row in 0..<8 {
                for px in 0..<8 {
                    bottomPixels[row][px] = pixels[row + 8][col * 8 + px]
                }
            }
            bottomTiles.append(GBTile8x8(pixels: bottomPixels))
        }

        return topTiles + bottomTiles
    }

    /// Number of tile columns in the composed sprite.
    var columnCount: Int {
        pixels.first.map { $0.count / 8 } ?? 1
    }
}
