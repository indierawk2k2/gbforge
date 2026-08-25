import AppKit
import Foundation

/// Imports PNG sprite sheets and converts to 2bpp Game Boy tiles.
/// Finds best palette for the whole 16x16 tile.
enum PNGImporter {
    static func importPNG(url: URL, into document: TileDocument) {
        guard url.startAccessingSecurityScopedResource() else { return }
        defer { url.stopAccessingSecurityScopedResource() }

        guard let image = NSImage(contentsOf: url),
              let cgImage = image.cgImage(forProposedRect: nil, context: nil, hints: nil)
        else { return }

        let width = cgImage.width
        let height = cgImage.height

        guard let pixels = extractPixels(from: cgImage) else { return }

        let tilesX = width / 16
        let tilesY = height / 16

        // Push undo snapshot before modifying document
        document.beginUndoGroup(actionKind: .pngImport)

        for ty in 0..<tilesY {
            for tx in 0..<tilesX {
                let tile = extractTile(
                    pixels: pixels,
                    imageWidth: width,
                    tileX: tx * 16,
                    tileY: ty * 16,
                    palettes: document.palettes
                )
                document.tiles.append(tile)
            }
        }

        if document.tiles.count > 1 {
            if document.tiles.count > tilesX * tilesY &&
               document.tiles[0].name == "Tile 0" &&
               document.tiles[0].subTiles.allSatisfy({ $0 == GBTile8x8() }) {
                document.tiles.removeFirst()
            }
            document.selectedTileIndex = 0
        }
    }

    private static func extractPixels(from cgImage: CGImage) -> [UInt8]? {
        let width = cgImage.width
        let height = cgImage.height
        let bytesPerPixel = 4
        let bytesPerRow = width * bytesPerPixel
        var pixels = [UInt8](repeating: 0, count: height * bytesPerRow)

        guard let context = CGContext(
            data: &pixels,
            width: width,
            height: height,
            bitsPerComponent: 8,
            bytesPerRow: bytesPerRow,
            space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
        ) else { return nil }

        context.draw(cgImage, in: CGRect(x: 0, y: 0, width: width, height: height))
        return pixels
    }

    /// Find the best matching palette for a 16x16 region of pixels.
    private static func bestPalette(
        regionColors: [[GBColor]],
        palettes: [GBPalette]
    ) -> Int {
        var bestPal = 0
        var bestError = Int.max

        for (pi, palette) in palettes.enumerated() {
            var totalError = 0
            for row in regionColors {
                for c in row {
                    let minDist = palette.colors.map { c.distanceSq(to: $0) }.min() ?? Int.max
                    totalError += minDist
                }
            }
            if totalError < bestError {
                bestError = totalError
                bestPal = pi
            }
        }
        return bestPal
    }

    private static func extractTile(
        pixels: [UInt8],
        imageWidth: Int,
        tileX: Int,
        tileY: Int,
        palettes: [GBPalette]
    ) -> GBTile16x16 {
        // Collect RGB555 colors for the full 16x16 region
        var regionColors = [[GBColor]](repeating: [GBColor](repeating: .black, count: 16), count: 16)

        for row in 0..<16 {
            for col in 0..<16 {
                let px = tileX + col
                let py = tileY + row
                let offset = (py * imageWidth + px) * 4
                let r = pixels[offset]
                let g = pixels[offset + 1]
                let b = pixels[offset + 2]
                regionColors[row][col] = GBColor(
                    r: UInt8(min(Double(r) / 255.0 * 31.0 + 0.5, 31)),
                    g: UInt8(min(Double(g) / 255.0 * 31.0 + 0.5, 31)),
                    b: UInt8(min(Double(b) / 255.0 * 31.0 + 0.5, 31))
                )
            }
        }

        // Find best palette for the whole 16x16 tile
        let palIdx = bestPalette(regionColors: regionColors, palettes: palettes)
        let palette = palettes[palIdx]

        // Map each pixel to nearest color in the tile's palette
        var subTiles = [GBTile8x8(), GBTile8x8(), GBTile8x8(), GBTile8x8()]

        for row in 0..<16 {
            for col in 0..<16 {
                let q = GBTile16x16.quadrant(x: col, y: row)
                let c = regionColors[row][col]
                var bestIdx = 0
                var bestDist = Int.max
                for (ci, pc) in palette.colors.enumerated() {
                    let d = c.distanceSq(to: pc)
                    if d < bestDist {
                        bestDist = d
                        bestIdx = ci
                    }
                }
                subTiles[q].pixels[row % 8][col % 8] = UInt8(bestIdx)
            }
        }

        let name = "Import \(tileX/16),\(tileY/16)"
        return GBTile16x16(name: name, subTiles: subTiles, paletteIndex: palIdx)
    }
}
