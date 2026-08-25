import Foundation

/// A 4-color Game Boy Color palette.
struct GBPalette: Equatable, Codable {
    var name: String
    var colors: [GBColor]  // Exactly 4 colors, index 0 = lightest

    init(name: String = "Untitled", colors: [GBColor]) {
        assert(colors.count == 4)
        self.name = name
        self.colors = colors
    }

    /// Default grayscale palette.
    static let grayscale = GBPalette(name: "Grayscale", colors: [
        .white, .lightGray, .darkGray, .black
    ])

    subscript(index: Int) -> GBColor {
        get { colors[index] }
        set { colors[index] = newValue }
    }
}
