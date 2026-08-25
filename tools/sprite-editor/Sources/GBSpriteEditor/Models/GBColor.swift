import SwiftUI

/// A Game Boy Color RGB555 color (5 bits per channel, 0-31).
struct GBColor: Equatable, Codable {
    var r: UInt8  // 0-31
    var g: UInt8  // 0-31
    var b: UInt8  // 0-31

    init(r: UInt8, g: UInt8, b: UInt8) {
        self.r = min(r, 31)
        self.g = min(g, 31)
        self.b = min(b, 31)
    }

    /// Decode from GBC native uint16 format: 0bBBBBB_GGGGG_RRRRR
    init(raw: UInt16) {
        self.r = UInt8(raw & 0x1F)
        self.g = UInt8((raw >> 5) & 0x1F)
        self.b = UInt8((raw >> 10) & 0x1F)
    }

    /// Encode to GBC native uint16 format.
    var raw: UInt16 {
        UInt16(r) | (UInt16(g) << 5) | (UInt16(b) << 10)
    }

    /// SameBoy CGB "Emulate Hardware" gamma curve (5-bit → 8-bit).
    private static let gbcCurve: [UInt8] = [
        0,  6, 12, 20, 28, 36, 45, 56, 66, 76, 88,100,113,125,137,
      149,161,172,182,192,202,210,218,225,232,238,243,247,250,252,254,255
    ]

    /// Apply SameBoy's GBC color correction: gamma curve + green/blue blend.
    private var gbcDisplay: (r: Double, g: Double, b: Double) {
        let cr = Double(Self.gbcCurve[Int(r)]) / 255.0
        var cg = Double(Self.gbcCurve[Int(g)]) / 255.0
        let cb = Double(Self.gbcCurve[Int(b)]) / 255.0
        // Blend green toward blue using gamma-1.6 power mean, 3:1 ratio
        if g != b {
            let gamma = 1.6
            let gP = pow(cg, gamma)
            let bP = pow(cb, gamma)
            cg = pow((gP * 3.0 + bP) / 4.0, 1.0 / gamma)
        }
        return (cr, cg, cb)
    }

    /// Convert to SwiftUI Color for display, applying GBC color correction.
    var color: Color {
        let m = gbcDisplay
        return Color(red: m.r, green: m.g, blue: m.b)
    }

    /// Convert to NSColor for bitmap rendering, applying GBC color correction.
    var nsColor: NSColor {
        let m = gbcDisplay
        return NSColor(red: m.r, green: m.g, blue: m.b, alpha: 1.0)
    }

    /// Create from an arbitrary CGColor by quantizing to RGB555.
    init(cgColor: CGColor) {
        let c = NSColor(cgColor: cgColor)?.usingColorSpace(.sRGB)
            ?? NSColor.black
        self.r = UInt8(min(max(c.redComponent * 31.0 + 0.5, 0), 31))
        self.g = UInt8(min(max(c.greenComponent * 31.0 + 0.5, 0), 31))
        self.b = UInt8(min(max(c.blueComponent * 31.0 + 0.5, 0), 31))
    }

    /// Luminance for sorting (approximate).
    var luminance: Double {
        0.299 * Double(r) / 31.0 + 0.587 * Double(g) / 31.0 + 0.114 * Double(b) / 31.0
    }

    /// Squared distance to another color (for nearest-color matching).
    func distanceSq(to other: GBColor) -> Int {
        let dr = Int(r) - Int(other.r)
        let dg = Int(g) - Int(other.g)
        let db = Int(b) - Int(other.b)
        return dr * dr + dg * dg + db * db
    }

    static let white = GBColor(r: 31, g: 31, b: 31)
    static let lightGray = GBColor(r: 20, g: 20, b: 20)
    static let darkGray = GBColor(r: 10, g: 10, b: 10)
    static let black = GBColor(r: 0, g: 0, b: 0)
}
