import AppKit
import Foundation

/// Pastes an image from the clipboard into the selected tile,
/// with automatic pixel art detection for upscaled screenshots.
enum ClipboardImporter {

    /// Returns true if the pasteboard has an image.
    static var hasImage: Bool {
        NSPasteboard.general.canReadObject(forClasses: [NSImage.self], options: nil)
    }

    /// Paste clipboard image into the currently selected tile.
    static func paste(into document: TileDocument) {
        guard let items = NSPasteboard.general.readObjects(forClasses: [NSImage.self], options: nil),
              let image = items.first as? NSImage,
              let cgImage = image.cgImage(forProposedRect: nil, context: nil, hints: nil)
        else { return }

        let srcW = cgImage.width
        let srcH = cgImage.height
        guard let rgba = extractRGBA(from: cgImage) else { return }

        // Try pixel art detection first
        let pixelSize = detectPixelSize(rgba: rgba, width: srcW, height: srcH)

        let colors: [[GBColor]]
        let dstW: Int
        let dstH: Int

        if pixelSize > 1 {
            // Pixel art mode: downsample using detected grid
            dstW = srcW / pixelSize
            dstH = srcH / pixelSize
            guard dstW > 0, dstH > 0 else { return }

            var grid = [[GBColor]](repeating: [GBColor](repeating: .black, count: dstW), count: dstH)
            for row in 0..<dstH {
                for col in 0..<dstW {
                    grid[row][col] = modeColor(
                        rgba: rgba, imgW: srcW,
                        blockX: col * pixelSize, blockY: row * pixelSize,
                        blockSize: pixelSize
                    )
                }
            }
            colors = grid
        } else {
            // Normal mode: resize to fit, longest side = 16
            if srcW >= srcH {
                dstW = 16
                dstH = max(1, Int(round(Double(srcH) * 16.0 / Double(srcW))))
            } else {
                dstH = 16
                dstW = max(1, Int(round(Double(srcW) * 16.0 / Double(srcH))))
            }

            guard let resized = resize(cgImage, toWidth: dstW, height: dstH) else { return }
            guard let resizedRGBA = extractRGBA(from: resized) else { return }

            var grid = [[GBColor]](repeating: [GBColor](repeating: .black, count: dstW), count: dstH)
            for row in 0..<dstH {
                for col in 0..<dstW {
                    let offset = (row * dstW + col) * 4
                    grid[row][col] = GBColor(
                        r: UInt8(min(Double(resizedRGBA[offset]) / 255.0 * 31.0 + 0.5, 31)),
                        g: UInt8(min(Double(resizedRGBA[offset + 1]) / 255.0 * 31.0 + 0.5, 31)),
                        b: UInt8(min(Double(resizedRGBA[offset + 2]) / 255.0 * 31.0 + 0.5, 31))
                    )
                }
            }
            colors = grid
        }

        // Collect all colors for palette generation
        let allColors = colors.flatMap { $0 }

        // Quantize to a single 4-color palette via median cut
        let palette = medianCut(colors: allColors, targetCount: 4)

        // Push undo snapshot before modifying tile data and palette
        document.beginUndoGroup(actionKind: .paste)

        // Overwrite the tile's current palette colors in-place
        let idx = document.selectedTileIndex
        let palIdx = document.tiles[idx].paletteIndex
        document.palettes[palIdx].colors = palette.colors

        // Map each pixel to nearest palette color, placed at top-left of 16x16 tile
        var subTiles = [GBTile8x8(), GBTile8x8(), GBTile8x8(), GBTile8x8()]

        for row in 0..<min(dstH, 16) {
            for col in 0..<min(dstW, 16) {
                let c = colors[row][col]
                var bestIdx = 0
                var bestDist = Int.max
                for (ci, pc) in palette.colors.enumerated() {
                    let d = c.distanceSq(to: pc)
                    if d < bestDist {
                        bestDist = d
                        bestIdx = ci
                    }
                }
                let q = GBTile16x16.quadrant(x: col, y: row)
                subTiles[q].pixels[row % 8][col % 8] = UInt8(bestIdx)
            }
        }

        // Replace the selected tile's pixel data (palette index stays the same)
        document.tiles[idx].subTiles = subTiles
    }

    /// Paste clipboard image into the currently selected tile,
    /// mapping each pixel to the closest color in the tile's EXISTING palette.
    /// The palette is NOT modified.
    static func pasteMatchingPalette(into document: TileDocument) {
        guard let items = NSPasteboard.general.readObjects(forClasses: [NSImage.self], options: nil),
              let image = items.first as? NSImage,
              let cgImage = image.cgImage(forProposedRect: nil, context: nil, hints: nil)
        else { return }

        let srcW = cgImage.width
        let srcH = cgImage.height
        guard let rgba = extractRGBA(from: cgImage) else { return }

        // Try pixel art detection first
        let pixelSize = detectPixelSize(rgba: rgba, width: srcW, height: srcH)

        let colors: [[GBColor]]
        let dstW: Int
        let dstH: Int

        if pixelSize > 1 {
            // Pixel art mode: downsample using detected grid
            dstW = srcW / pixelSize
            dstH = srcH / pixelSize
            guard dstW > 0, dstH > 0 else { return }

            var grid = [[GBColor]](repeating: [GBColor](repeating: .black, count: dstW), count: dstH)
            for row in 0..<dstH {
                for col in 0..<dstW {
                    grid[row][col] = modeColor(
                        rgba: rgba, imgW: srcW,
                        blockX: col * pixelSize, blockY: row * pixelSize,
                        blockSize: pixelSize
                    )
                }
            }
            colors = grid
        } else {
            // Normal mode: resize to fit, longest side = 16
            if srcW >= srcH {
                dstW = 16
                dstH = max(1, Int(round(Double(srcH) * 16.0 / Double(srcW))))
            } else {
                dstH = 16
                dstW = max(1, Int(round(Double(srcW) * 16.0 / Double(srcH))))
            }

            guard let resized = resize(cgImage, toWidth: dstW, height: dstH) else { return }
            guard let resizedRGBA = extractRGBA(from: resized) else { return }

            var grid = [[GBColor]](repeating: [GBColor](repeating: .black, count: dstW), count: dstH)
            for row in 0..<dstH {
                for col in 0..<dstW {
                    let offset = (row * dstW + col) * 4
                    grid[row][col] = GBColor(
                        r: UInt8(min(Double(resizedRGBA[offset]) / 255.0 * 31.0 + 0.5, 31)),
                        g: UInt8(min(Double(resizedRGBA[offset + 1]) / 255.0 * 31.0 + 0.5, 31)),
                        b: UInt8(min(Double(resizedRGBA[offset + 2]) / 255.0 * 31.0 + 0.5, 31))
                    )
                }
            }
            colors = grid
        }

        // Use the tile's existing palette — do NOT modify it
        let idx = document.selectedTileIndex
        let palIdx = document.tiles[idx].paletteIndex
        let palette = document.palettes[palIdx]

        // Push undo snapshot before modifying tile data
        document.beginUndoGroup(actionKind: .pasteMatchPalette)

        // Map each pixel to nearest existing palette color, placed at top-left of 16x16 tile
        var subTiles = [GBTile8x8(), GBTile8x8(), GBTile8x8(), GBTile8x8()]

        for row in 0..<min(dstH, 16) {
            for col in 0..<min(dstW, 16) {
                let c = colors[row][col]
                var bestIdx = 0
                var bestDist = Int.max
                for (ci, pc) in palette.colors.enumerated() {
                    let d = c.distanceSq(to: pc)
                    if d < bestDist {
                        bestDist = d
                        bestIdx = ci
                    }
                }
                let q = GBTile16x16.quadrant(x: col, y: row)
                subTiles[q].pixels[row % 8][col % 8] = UInt8(bestIdx)
            }
        }

        // Replace the selected tile's pixel data (palette stays unchanged)
        document.tiles[idx].subTiles = subTiles
    }

    // MARK: - Pixel art detection

    /// Detect pixel block size via GCD of color-transition gaps.
    /// Returns 1 if no pixel grid is detected.
    private static func detectPixelSize(rgba: [UInt8], width: Int, height: Int) -> Int {
        let numSamples = min(20, max(height, width))
        var gapGCD = 0

        // Sample rows
        for si in 0..<min(numSamples, height) {
            let row = si * height / numSamples
            for gap in transitionGaps(rgba: rgba, width: width, height: height, isRow: true, index: row) {
                gapGCD = gcd(gapGCD, gap)
            }
        }

        // Sample columns
        for si in 0..<min(numSamples, width) {
            let col = si * width / numSamples
            for gap in transitionGaps(rgba: rgba, width: width, height: height, isRow: false, index: col) {
                gapGCD = gcd(gapGCD, gap)
            }
        }

        let candidate = max(1, gapGCD)
        if candidate <= 1 { return 1 }

        // Verify: sample grid-aligned blocks, check >85% are >90% uniform
        if verifyPixelSize(rgba: rgba, width: width, height: height, size: candidate) {
            return candidate
        }
        return 1
    }

    /// Find gaps between color transitions along a row or column.
    private static func transitionGaps(rgba: [UInt8], width: Int, height: Int,
                                        isRow: Bool, index: Int) -> [Int] {
        let length = isRow ? width : height
        var gaps: [Int] = []
        var lastTransition = 0

        for i in 1..<length {
            let (r1, g1, b1) = pixelAt(rgba: rgba, width: width,
                                         x: isRow ? i - 1 : index,
                                         y: isRow ? index : i - 1)
            let (r2, g2, b2) = pixelAt(rgba: rgba, width: width,
                                         x: isRow ? i : index,
                                         y: isRow ? index : i)
            let dr = abs(Int(r1) - Int(r2))
            let dg = abs(Int(g1) - Int(g2))
            let db = abs(Int(b1) - Int(b2))
            if dr + dg + db > 30 {
                let gap = i - lastTransition
                if gap > 1 {
                    gaps.append(gap)
                }
                lastTransition = i
            }
        }
        let finalGap = length - lastTransition
        if finalGap > 1 {
            gaps.append(finalGap)
        }
        return gaps
    }

    /// Verify that SxS blocks are mostly uniform in color.
    private static func verifyPixelSize(rgba: [UInt8], width: Int, height: Int, size: Int) -> Bool {
        let gridW = width / size
        let gridH = height / size
        let totalBlocks = gridW * gridH
        let sampleCount = min(100, totalBlocks)
        guard sampleCount > 0 else { return false }

        let step = max(1, totalBlocks / sampleCount)
        var uniformCount = 0
        var checked = 0

        for i in stride(from: 0, to: totalBlocks, by: step) {
            let gx = i % gridW
            let gy = i / gridW
            if isBlockUniform(rgba: rgba, width: width, height: height,
                              blockX: gx * size, blockY: gy * size, blockSize: size) {
                uniformCount += 1
            }
            checked += 1
        }

        guard checked > 0 else { return false }
        return Double(uniformCount) / Double(checked) > 0.85
    }

    /// Check if >90% of pixels in a block share the same quantized color.
    private static func isBlockUniform(rgba: [UInt8], width: Int, height: Int,
                                        blockX: Int, blockY: Int, blockSize: Int) -> Bool {
        var colorCounts: [UInt32: Int] = [:]
        var total = 0

        for dy in 0..<blockSize {
            for dx in 0..<blockSize {
                let px = blockX + dx
                let py = blockY + dy
                guard px < width, py < height else { continue }
                let (r, g, b) = pixelAt(rgba: rgba, width: width, x: px, y: py)
                let key = (UInt32(r / 16) << 8) | (UInt32(g / 16) << 4) | UInt32(b / 16)
                colorCounts[key, default: 0] += 1
                total += 1
            }
        }

        guard total > 0 else { return true }
        let maxCount = colorCounts.values.max() ?? 0
        return Double(maxCount) / Double(total) > 0.90
    }

    /// Get mode (most common) color in a block, return as GBColor.
    private static func modeColor(rgba: [UInt8], imgW: Int,
                                   blockX: Int, blockY: Int, blockSize: Int) -> GBColor {
        var colorCounts: [UInt32: (count: Int, r: Int, g: Int, b: Int)] = [:]

        for dy in 0..<blockSize {
            for dx in 0..<blockSize {
                let px = blockX + dx
                let py = blockY + dy
                let (r, g, b) = pixelAt(rgba: rgba, width: imgW, x: px, y: py)
                let key = (UInt32(r) << 16) | (UInt32(g) << 8) | UInt32(b)
                if var entry = colorCounts[key] {
                    entry.count += 1
                    colorCounts[key] = entry
                } else {
                    colorCounts[key] = (count: 1, r: Int(r), g: Int(g), b: Int(b))
                }
            }
        }

        guard let best = colorCounts.values.max(by: { $0.count < $1.count }) else {
            return .black
        }

        return GBColor(
            r: UInt8(min(Double(best.r) / 255.0 * 31.0 + 0.5, 31)),
            g: UInt8(min(Double(best.g) / 255.0 * 31.0 + 0.5, 31)),
            b: UInt8(min(Double(best.b) / 255.0 * 31.0 + 0.5, 31))
        )
    }

    private static func pixelAt(rgba: [UInt8], width: Int, x: Int, y: Int) -> (UInt8, UInt8, UInt8) {
        let offset = (y * width + x) * 4
        guard offset + 2 < rgba.count else { return (0, 0, 0) }
        return (rgba[offset], rgba[offset + 1], rgba[offset + 2])
    }

    private static func gcd(_ a: Int, _ b: Int) -> Int {
        if b == 0 { return a }
        return gcd(b, a % b)
    }

    // MARK: - Image helpers

    private static func resize(_ source: CGImage, toWidth w: Int, height h: Int) -> CGImage? {
        let context = CGContext(
            data: nil,
            width: w,
            height: h,
            bitsPerComponent: 8,
            bytesPerRow: w * 4,
            space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
        )
        context?.interpolationQuality = .none
        context?.draw(source, in: CGRect(x: 0, y: 0, width: w, height: h))
        return context?.makeImage()
    }

    private static func extractRGBA(from cgImage: CGImage) -> [UInt8]? {
        let w = cgImage.width
        let h = cgImage.height
        var pixels = [UInt8](repeating: 0, count: w * h * 4)
        guard let ctx = CGContext(
            data: &pixels,
            width: w,
            height: h,
            bitsPerComponent: 8,
            bytesPerRow: w * 4,
            space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
        ) else { return nil }
        ctx.draw(cgImage, in: CGRect(x: 0, y: 0, width: w, height: h))
        return pixels
    }

    // MARK: - Median cut quantization

    /// Reduce a list of colors to exactly `targetCount` representative colors
    /// using median-cut, then return a GBPalette sorted by luminance.
    private static func medianCut(colors: [GBColor], targetCount: Int) -> GBPalette {
        var unique = [GBColor]()
        var seen = Set<UInt16>()
        for c in colors {
            if seen.insert(c.raw).inserted {
                unique.append(c)
            }
        }

        if unique.count <= targetCount {
            var result = unique
            while result.count < targetCount {
                result.append(result.isEmpty ? .black : result.last!)
            }
            result.sort { $0.luminance > $1.luminance }
            return GBPalette(colors: Array(result.prefix(4)))
        }

        var buckets: [[GBColor]] = [unique]

        while buckets.count < targetCount {
            var bestBucket = 0
            var bestRange = -1

            for (i, bucket) in buckets.enumerated() {
                let rRange = Int(bucket.map(\.r).max()!) - Int(bucket.map(\.r).min()!)
                let gRange = Int(bucket.map(\.g).max()!) - Int(bucket.map(\.g).min()!)
                let bRange = Int(bucket.map(\.b).max()!) - Int(bucket.map(\.b).min()!)
                let maxRange = max(rRange, gRange, bRange)
                if maxRange > bestRange && bucket.count >= 2 {
                    bestRange = maxRange
                    bestBucket = i
                }
            }

            if bestRange <= 0 { break }

            let bucket = buckets[bestBucket]
            let rRange = Int(bucket.map(\.r).max()!) - Int(bucket.map(\.r).min()!)
            let gRange = Int(bucket.map(\.g).max()!) - Int(bucket.map(\.g).min()!)
            let bRange = Int(bucket.map(\.b).max()!) - Int(bucket.map(\.b).min()!)

            let sorted: [GBColor]
            if rRange >= gRange && rRange >= bRange {
                sorted = bucket.sorted { $0.r < $1.r }
            } else if gRange >= bRange {
                sorted = bucket.sorted { $0.g < $1.g }
            } else {
                sorted = bucket.sorted { $0.b < $1.b }
            }

            let mid = sorted.count / 2
            buckets[bestBucket] = Array(sorted[..<mid])
            buckets.append(Array(sorted[mid...]))
        }

        var result = buckets.map { bucket -> GBColor in
            let count = bucket.count
            let r = bucket.reduce(0) { $0 + Int($1.r) } / count
            let g = bucket.reduce(0) { $0 + Int($1.g) } / count
            let b = bucket.reduce(0) { $0 + Int($1.b) } / count
            return GBColor(r: UInt8(r), g: UInt8(g), b: UInt8(b))
        }

        while result.count < targetCount {
            result.append(result.last ?? .black)
        }

        result.sort { $0.luminance > $1.luminance }
        return GBPalette(name: "Pasted", colors: Array(result.prefix(4)))
    }
}
