import AppKit
import Foundation

/// A single character glyph extracted from the pixel font sheet.
struct PixelGlyph {
    let character: Character
    /// Row-major pixel grid (true = ink, false = background).
    let pixels: [[Bool]]
    var width: Int { pixels.first?.count ?? 0 }
    var height: Int { pixels.count }
}

/// Parses the DropShadowPixelFont.jpg image to extract individual character glyphs.
/// Uses luminance-based analysis to handle JPEG compression artifacts — the font
/// image has drop shadows that bridge between adjacent characters, so simple
/// column-gap segmentation won't work. Instead, we use known character counts
/// per row and constrained even splitting with snap-to-brightest-column.
///
/// Singleton: parsed once at launch, shared by all text sprites.
class PixelFont {
    static let shared = PixelFont()

    private(set) var glyphs: [Character: PixelGlyph] = [:]
    private(set) var glyphHeight: Int = 0

    private init() {
        loadFont()
    }

    // MARK: - Font loading pipeline

    private func loadFont() {
        let sourceFile = URL(fileURLWithPath: #filePath)
        let repoRoot = sourceFile
            .deletingLastPathComponent()  // Models/
            .deletingLastPathComponent()  // GBSpriteEditor/
            .deletingLastPathComponent()  // Sources/
            .deletingLastPathComponent()  // sprite-editor/
            .deletingLastPathComponent()  // repo root
        let fontURL = repoRoot.appendingPathComponent("DropShadowPixelFont.jpg")

        guard let nsImage = NSImage(contentsOf: fontURL),
              let cgImage = nsImage.cgImage(forProposedRect: nil, context: nil, hints: nil) else {
            print("Warning: could not load font image at \(fontURL.path)")
            return
        }

        let width = cgImage.width
        let height = cgImage.height
        let bpp = 4
        var pixelData = [UInt8](repeating: 0, count: width * height * bpp)

        guard let context = CGContext(
            data: &pixelData,
            width: width,
            height: height,
            bitsPerComponent: 8,
            bytesPerRow: width * bpp,
            space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
        ) else { return }

        context.draw(cgImage, in: CGRect(x: 0, y: 0, width: width, height: height))

        // Build luminance grid (0-255, higher = brighter)
        var lum = [[Int]](repeating: [Int](repeating: 0, count: width), count: height)
        for y in 0..<height {
            for x in 0..<width {
                let off = (y * width + x) * bpp
                let r = Int(pixelData[off])
                let g = Int(pixelData[off + 1])
                let b = Int(pixelData[off + 2])
                lum[y][x] = (r * 299 + g * 587 + b * 114) / 1000
            }
        }

        // Step 1: Find row bands (rows where any column has dark pixels)
        let bands = findRowBands(lum: lum, width: width, height: height, threshold: 128)
        guard bands.count >= 7 else {
            print("Warning: expected 7 row bands, found \(bands.count)")
            return
        }

        // Step 2: Estimate downscale factor from band heights
        // Native glyph height is ~12-13px. Band heights are ~50px at ~4x scale.
        let avgBandHeight = Double(bands.map { $0.end - $0.start }.reduce(0, +)) / Double(bands.count)
        let scale = max(1, Int((avgBandHeight / 10.0).rounded()))

        // Step 3: Extract characters from each band
        // Band 0: ABCDEFGHIJ, Band 1: KLMNOPQRST, Band 2: UVWXYZ.?
        // Bands 3-5: duplicate variant (skip, but check band 5 for '!')
        // Band 6: 0123456789
        let bandMappings: [(band: Int, chars: [Character])] = [
            (0, Array("ABCDEFGHIJ")),
            (1, Array("KLMNOPQRST")),
            (2, Array("UVWXYZ.?")),
            (6, Array("0123456789")),
        ]

        for mapping in bandMappings {
            guard mapping.band < bands.count else { continue }
            let band = bands[mapping.band]
            let charCount = mapping.chars.count
            let colAvg = columnAvgLuminance(lum: lum, band: band, width: width)
            let region = findInkRegion(colAvg: colAvg, threshold: 200)
            let splits = findConstrainedSplits(
                colAvg: colAvg, region: region, charCount: charCount
            )
            let boundaries = [region.start] + splits + [region.end]

            for i in 0..<charCount {
                let glyph = extractAndDownsample(
                    lum: lum, band: band,
                    colStart: boundaries[i], colEnd: boundaries[i + 1],
                    scale: scale, character: mapping.chars[i]
                )
                glyphs[mapping.chars[i]] = glyph
            }
        }

        // Extract '!' from band 5 (last character in the duplicate variant row)
        if bands.count > 5 {
            let band = bands[5]
            let colAvg = columnAvgLuminance(lum: lum, band: band, width: width)
            let region = findInkRegion(colAvg: colAvg, threshold: 200)
            // Band 5 has "TUVWXYZ" + gap + "!" — split into 8 characters
            let splits = findConstrainedSplits(colAvg: colAvg, region: region, charCount: 8)
            let boundaries = [region.start] + splits + [region.end]
            if boundaries.count > 8 {
                let glyph = extractAndDownsample(
                    lum: lum, band: band,
                    colStart: boundaries[7], colEnd: boundaries[8],
                    scale: scale, character: "!"
                )
                glyphs["!"] = glyph
            }
        }

        // Synthesize missing characters
        synthesizeMissingCharacters()

        // Set common glyph height from any extracted glyph
        if let anyGlyph = glyphs.values.first {
            glyphHeight = anyGlyph.height
        }
    }

    // MARK: - Row band detection

    private struct Band {
        let start: Int
        let end: Int
    }

    private func findRowBands(lum: [[Int]], width: Int, height: Int, threshold: Int) -> [Band] {
        var bands: [Band] = []
        var inBand = false
        var bandStart = 0

        for y in 0..<height {
            var hasDark = false
            for x in 0..<width {
                if lum[y][x] < threshold { hasDark = true; break }
            }
            if hasDark && !inBand {
                bandStart = y
                inBand = true
            } else if !hasDark && inBand {
                bands.append(Band(start: bandStart, end: y))
                inBand = false
            }
        }
        if inBand {
            bands.append(Band(start: bandStart, end: height))
        }
        return bands
    }

    // MARK: - Column luminance analysis

    private func columnAvgLuminance(lum: [[Int]], band: Band, width: Int) -> [Double] {
        let bandHeight = Double(band.end - band.start)
        return (0..<width).map { x in
            var sum = 0
            for y in band.start..<band.end { sum += lum[y][x] }
            return Double(sum) / bandHeight
        }
    }

    private func findInkRegion(colAvg: [Double], threshold: Double) -> (start: Int, end: Int) {
        var first = colAvg.count
        var last = 0
        for x in 0..<colAvg.count {
            if colAvg[x] < threshold {
                first = min(first, x)
                last = max(last, x)
            }
        }
        return (first, last + 1)
    }

    /// Divide the ink region into `charCount` segments. Start with even spacing,
    /// then snap each split point to the brightest nearby column (+/- 8px).
    private func findConstrainedSplits(
        colAvg: [Double],
        region: (start: Int, end: Int),
        charCount: Int,
        snapRange: Int = 8
    ) -> [Int] {
        let regionWidth = region.end - region.start
        let charWidth = Double(regionWidth) / Double(charCount)

        var splits: [Int] = []
        for i in 1..<charCount {
            let idealX = region.start + Int(charWidth * Double(i))
            let lo = max(region.start, idealX - snapRange)
            let hi = min(region.end, idealX + snapRange)
            var bestX = idealX
            var bestLum = -1.0
            for x in lo..<hi {
                if colAvg[x] > bestLum {
                    bestLum = colAvg[x]
                    bestX = x
                }
            }
            splits.append(bestX)
        }
        return splits
    }

    // MARK: - Glyph extraction with downsampling

    /// Extract a character region and downsample by `scale` using block luminance averaging.
    private func extractAndDownsample(
        lum: [[Int]], band: Band,
        colStart: Int, colEnd: Int,
        scale: Int, character: Character
    ) -> PixelGlyph {
        let cellW = colEnd - colStart
        let cellH = band.end - band.start
        let dsW = cellW / scale
        let dsH = cellH / scale

        var pixels = [[Bool]](repeating: [Bool](repeating: false, count: dsW), count: dsH)
        for dy in 0..<dsH {
            for dx in 0..<dsW {
                var sum = 0
                var count = 0
                for sy in 0..<scale {
                    let y = band.start + dy * scale + sy
                    guard y < band.end else { continue }
                    for sx in 0..<scale {
                        let x = colStart + dx * scale + sx
                        guard x < colEnd else { continue }
                        sum += lum[y][x]
                        count += 1
                    }
                }
                let avg = count > 0 ? sum / count : 255
                pixels[dy][dx] = avg < 128
            }
        }
        return PixelGlyph(character: character, pixels: pixels)
    }

    // MARK: - Synthesized characters

    private func synthesizeMissingCharacters() {
        // Synthesize '-' (horizontal bar at midpoint)
        if glyphs["-"] == nil, let ref = glyphs["A"] {
            let h = ref.height
            let w = max(4, ref.width / 2)
            var pixels = [[Bool]](repeating: [Bool](repeating: false, count: w), count: h)
            let midY = h / 2
            for dy in max(0, midY - 1)...min(h - 1, midY) {
                for dx in 0..<w { pixels[dy][dx] = true }
            }
            glyphs["-"] = PixelGlyph(character: "-", pixels: pixels)
        }

        // Synthesize ' ' (empty space)
        if glyphs[" "] == nil, let ref = glyphs["A"] {
            let h = ref.height
            let pixels = [[Bool]](repeating: [Bool](repeating: false, count: 4), count: h)
            glyphs[" "] = PixelGlyph(character: " ", pixels: pixels)
        }
    }
}
