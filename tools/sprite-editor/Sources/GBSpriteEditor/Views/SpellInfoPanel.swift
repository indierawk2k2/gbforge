import SwiftUI

/// Right panel showing spell name, read-only palette swatches, and previews.
/// Adapts for 8x8 spell icons and 32x32 cast tiles.
struct SpellInfoPanel: View {
    @ObservedObject var document: TileDocument

    private static let spellNames = ["Spark", "Gaia", "Aqua", "Tectonic", "Slide", "Dust", "1Up"]

    private var isCast: Bool { document.isSpellCastSelected }

    private var spellKind: SpellKind {
        if isCast {
            return SpellKind(rawValue: document.selectedSpellIndex - 14)!
        }
        return SpellKind(rawValue: document.selectedSpellIndex % 7)!
    }

    private var isDisabled: Bool {
        !isCast && document.selectedSpellIndex >= 7
    }

    private var palette: GBPalette {
        if isCast {
            return document.palettes[spellKind.paletteIndex]
        }
        return isDisabled ? GBPalette.grayscale : document.palettes[spellKind.paletteIndex]
    }

    private var variantLabel: String {
        if isCast { return "Cast" }
        return isDisabled ? "Disabled" : "Normal"
    }

    /// Pixel dimension of the tile being edited (8 for icons, 32 for cast).
    private var tileDim: Int { isCast ? 32 : 8 }

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            // Spell name + variant label
            Text("\(Self.spellNames[spellKind.rawValue]) \u{2014} \(variantLabel)")
                .font(.headline)

            // Read-only palette swatches
            VStack(alignment: .leading, spacing: 4) {
                Text("Palette: \(palette.name)")
                    .font(.caption)
                    .foregroundStyle(.secondary)

                HStack(spacing: 4) {
                    ForEach(0..<4, id: \.self) { i in
                        Rectangle()
                            .fill(palette[i].color)
                            .frame(width: 28, height: 28)
                            .border(Color.gray.opacity(0.5), width: 1)
                            .overlay(
                                Text("\(i)")
                                    .font(.caption2)
                                    .foregroundStyle(i < 2 ? .black : .white)
                            )
                    }
                }
            }

            // Actual-size previews
            VStack(alignment: .leading, spacing: 8) {
                Text("Preview")
                    .font(.caption)
                    .foregroundStyle(.secondary)

                if isCast {
                    HStack(spacing: 16) {
                        VStack(spacing: 2) {
                            Text("1x").font(.caption2).foregroundStyle(.secondary)
                            spellPreview(scale: 1)
                                .frame(width: 32, height: 32)
                        }
                        VStack(spacing: 2) {
                            Text("2x").font(.caption2).foregroundStyle(.secondary)
                            spellPreview(scale: 2)
                                .frame(width: 64, height: 64)
                        }
                    }
                    HStack(spacing: 16) {
                        VStack(spacing: 2) {
                            Text("4x").font(.caption2).foregroundStyle(.secondary)
                            spellPreview(scale: 4)
                                .frame(width: 128, height: 128)
                        }
                    }
                } else {
                    HStack(spacing: 16) {
                        VStack(spacing: 2) {
                            Text("1x").font(.caption2).foregroundStyle(.secondary)
                            spellPreview(scale: 1)
                                .frame(width: 8, height: 8)
                        }
                        VStack(spacing: 2) {
                            Text("2x").font(.caption2).foregroundStyle(.secondary)
                            spellPreview(scale: 2)
                                .frame(width: 16, height: 16)
                        }
                        VStack(spacing: 2) {
                            Text("4x").font(.caption2).foregroundStyle(.secondary)
                            spellPreview(scale: 4)
                                .frame(width: 32, height: 32)
                        }
                    }
                }
            }

            Spacer()
        }
        .padding()
        .frame(width: 180)
    }

    private func spellPreview(scale: Int) -> some View {
        let dim = tileDim
        let pal = palette
        let size = CGFloat(dim * scale)
        let c0 = pal[0].color
        let c1 = pal[1].color
        let c2 = pal[2].color
        let c3 = pal[3].color
        return Canvas { context, canvasSize in
            let px = canvasSize.width / CGFloat(dim)
            let py = canvasSize.height / CGFloat(dim)
            let colors = [c0, c1, c2, c3]
            for row in 0..<dim {
                for col in 0..<dim {
                    let ci: Int
                    if isCast {
                        ci = Int(document.spellCastTiles.tiles[document.selectedSpellIndex - 14].pixel(x: col, y: row))
                    } else {
                        ci = Int(document.spellIcons.icons[document.selectedSpellIndex].pixels[row][col])
                    }
                    let rect = CGRect(
                        x: CGFloat(col) * px,
                        y: CGFloat(row) * py,
                        width: px + 0.5,
                        height: py + 0.5
                    )
                    context.fill(Path(rect), with: .color(colors[ci]))
                }
            }
        }
        .frame(width: size, height: size)
    }
}
