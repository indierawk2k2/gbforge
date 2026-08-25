import SwiftUI

/// Sidebar showing spell icons and cast tiles in a 3-column grid (normal | disabled | cast).
/// Rows: Spark, Gaia, Aqua, Tectonic, Slide, Dust, 1Up.
struct SpellListView: View {
    @ObservedObject var document: TileDocument

    private static let spellNames = ["Spark", "Gaia", "Aqua", "Tectonic", "Slide", "Dust", "1Up"]
    private let thumbSize: CGFloat = 40

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Text("Spells")
                .font(.headline)
                .padding(.horizontal, 12)
                .padding(.top, 8)
                .padding(.bottom, 4)

            // Column headers
            HStack(spacing: 0) {
                Text("")
                    .frame(width: 50, alignment: .leading)
                Text("Normal")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .frame(width: thumbSize, alignment: .center)
                Spacer().frame(width: 4)
                Text("Off")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .frame(width: thumbSize, alignment: .center)
                Spacer().frame(width: 4)
                Text("Cast")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .frame(width: thumbSize, alignment: .center)
            }
            .padding(.horizontal, 8)
            .padding(.bottom, 2)

            ScrollView {
                VStack(spacing: 4) {
                    ForEach(0..<7, id: \.self) { spellRaw in
                        let kind = SpellKind(rawValue: spellRaw)!
                        let normalIndex = spellRaw
                        let disabledIndex = spellRaw + 7
                        let castIndex = spellRaw + 14

                        HStack(spacing: 0) {
                            Text(Self.spellNames[spellRaw])
                                .font(.caption)
                                .frame(width: 50, alignment: .leading)

                            spellThumbnail(index: normalIndex, kind: kind)
                                .frame(width: thumbSize, height: thumbSize)
                                .onTapGesture {
                                    document.selectedSpellIndex = normalIndex
                                }

                            Spacer().frame(width: 4)

                            spellThumbnail(index: disabledIndex, kind: kind)
                                .frame(width: thumbSize, height: thumbSize)
                                .onTapGesture {
                                    document.selectedSpellIndex = disabledIndex
                                }

                            Spacer().frame(width: 4)

                            castThumbnail(index: castIndex, kind: kind)
                                .frame(width: thumbSize, height: thumbSize)
                                .onTapGesture {
                                    document.selectedSpellIndex = castIndex
                                }
                        }
                        .padding(.horizontal, 8)
                    }
                }
                .padding(.vertical, 4)
            }
        }
        .frame(width: 210)
    }

    private func spellThumbnail(index: Int, kind: SpellKind) -> some View {
        let icon = document.spellIcons.icons[index]
        let isDisabled = index >= 7
        let palette: GBPalette = isDisabled ? GBPalette.grayscale : document.palettes[kind.paletteIndex]
        let isSelected = document.selectedSpellIndex == index
        let c0 = palette[0].color
        let c1 = palette[1].color
        let c2 = palette[2].color
        let c3 = palette[3].color

        return Canvas { context, size in
            let px = size.width / 8
            let py = size.height / 8
            let colors = [c0, c1, c2, c3]
            for row in 0..<8 {
                for col in 0..<8 {
                    let ci = Int(icon.pixels[row][col])
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
        .id(index)
        .border(isSelected ? Color.accentColor : Color.gray.opacity(0.3), width: isSelected ? 2 : 1)
    }

    private func castThumbnail(index: Int, kind: SpellKind) -> some View {
        let castTile = document.spellCastTiles.tiles[index - 14]
        let palette = document.palettes[kind.paletteIndex]
        let isSelected = document.selectedSpellIndex == index
        let c0 = palette[0].color
        let c1 = palette[1].color
        let c2 = palette[2].color
        let c3 = palette[3].color

        return Canvas { context, size in
            let px = size.width / 32
            let py = size.height / 32
            let colors = [c0, c1, c2, c3]
            for row in 0..<32 {
                for col in 0..<32 {
                    let ci = Int(castTile.pixel(x: col, y: row))
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
        .id(index)
        .border(isSelected ? Color.accentColor : Color.gray.opacity(0.3), width: isSelected ? 2 : 1)
    }
}
