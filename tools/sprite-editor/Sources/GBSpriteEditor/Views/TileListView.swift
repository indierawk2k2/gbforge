import SwiftUI

/// Sidebar list of all tiles with small previews.
struct TileListView: View {
    @ObservedObject var document: TileDocument

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Text("Tiles")
                .font(.headline)
                .padding(.horizontal, 12)
                .padding(.top, 8)
                .padding(.bottom, 4)

            List(selection: tileSelectionBinding) {
                ForEach(Array(document.tiles.enumerated()), id: \.element.id) { index, tile in
                    tileRow(tile: tile, index: index)
                        .tag(index)
                }
            }
            .listStyle(.sidebar)

            Divider()

            HStack {
                Button(action: { document.addTile() }) {
                    Label("Add", systemImage: "plus")
                }
                Spacer()
                Button(action: {
                    document.deleteTile(at: document.selectedTileIndex)
                }) {
                    Label("Delete", systemImage: "minus")
                }
                .disabled(document.tiles.count <= 1)
            }
            .padding(8)

            Divider()

            // Indicator tiles section
            Text("Indicators")
                .font(.headline)
                .padding(.horizontal, 12)
                .padding(.top, 8)
                .padding(.bottom, 4)

            VStack(spacing: 4) {
                ForEach(IndicatorKind.allCases, id: \.rawValue) { kind in
                    indicatorRow(kind: kind)
                }
            }
            .padding(.horizontal, 8)
            .padding(.bottom, 8)
        }
        .frame(width: 180)
    }

    /// Binding that clears selectedIndicatorIndex when a 16x16 tile is selected.
    private var tileSelectionBinding: Binding<Int> {
        Binding(
            get: { document.selectedTileIndex },
            set: { newValue in
                document.selectedTileIndex = newValue
                document.selectedIndicatorIndex = nil
            }
        )
    }

    private func indicatorRow(kind: IndicatorKind) -> some View {
        let index = kind.rawValue
        let isSelected = document.selectedIndicatorIndex == index

        return HStack(spacing: 8) {
            indicatorThumbnail(kind: kind)
                .frame(width: 32, height: 32)
            VStack(alignment: .leading) {
                Text(kind.name)
                    .font(.caption)
                    .lineLimit(1)
                Text("P\(kind.paletteIndex)")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
            Spacer()
        }
        .padding(.vertical, 2)
        .padding(.horizontal, 4)
        .background(isSelected ? Color.accentColor.opacity(0.2) : Color.clear)
        .cornerRadius(4)
        .onTapGesture {
            document.selectedIndicatorIndex = index
        }
    }

    private func indicatorThumbnail(kind: IndicatorKind) -> some View {
        let tile = document.indicatorTiles.tiles[kind.rawValue]
        let palette = document.palettes[kind.paletteIndex]
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
                    let ci = Int(tile.pixels[row][col])
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
        .border(document.selectedIndicatorIndex == kind.rawValue ? Color.accentColor : Color.gray.opacity(0.3),
                width: document.selectedIndicatorIndex == kind.rawValue ? 2 : 1)
    }

    private func tileRow(tile: GBTile16x16, index: Int) -> some View {
        HStack(spacing: 8) {
            tilePreview(tile: tile)
                .frame(width: 32, height: 32)
            VStack(alignment: .leading) {
                Text(tile.name)
                    .font(.caption)
                    .lineLimit(1)
                Text("P\(tile.paletteIndex)")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
        }
        .padding(.vertical, 2)
    }

    private func tilePreview(tile: GBTile16x16) -> some View {
        Canvas { context, size in
            let px = size.width / 16
            let py = size.height / 16
            for row in 0..<16 {
                for col in 0..<16 {
                    let palette = document.palettes[tile.paletteIndex]
                    let ci = Int(tile.pixel(x: col, y: row))
                    let rect = CGRect(
                        x: CGFloat(col) * px,
                        y: CGFloat(row) * py,
                        width: px + 0.5,
                        height: py + 0.5
                    )
                    context.fill(Path(rect), with: .color(palette[ci].color))
                }
            }
        }
    }
}
