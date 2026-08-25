import SwiftUI

/// Left sidebar: scrollable list of text sprites with small previews.
struct TextSpriteListView: View {
    @ObservedObject var document: TileDocument

    private let thumbHeight: CGFloat = 24

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Text("Text Sprites")
                .font(.headline)
                .padding(.horizontal, 12)
                .padding(.top, 8)
                .padding(.bottom, 4)

            ScrollView {
                VStack(spacing: 2) {
                    ForEach(Array(document.textSprites.enumerated()), id: \.element.id) { index, sprite in
                        HStack(spacing: 8) {
                            textSpriteThumbnail(sprite: sprite)
                                .frame(height: thumbHeight)

                            VStack(alignment: .leading, spacing: 1) {
                                Text(sprite.name)
                                    .font(.caption)
                                    .lineLimit(1)
                                Text("\"\(sprite.text)\"")
                                    .font(.caption2)
                                    .foregroundStyle(.secondary)
                                    .lineLimit(1)
                            }

                            Spacer()
                        }
                        .padding(.horizontal, 8)
                        .padding(.vertical, 4)
                        .background(
                            document.selectedTextSpriteIndex == index
                                ? Color.accentColor.opacity(0.2)
                                : Color.clear
                        )
                        .cornerRadius(4)
                        .contentShape(Rectangle())
                        .onTapGesture {
                            document.selectedTextSpriteIndex = index
                        }
                    }
                }
                .padding(.vertical, 4)
            }

            Divider()

            HStack {
                Button("+") { document.addTextSprite() }
                Button("-") {
                    document.deleteTextSprite(at: document.selectedTextSpriteIndex)
                }
                .disabled(document.textSprites.isEmpty)
                Spacer()
            }
            .padding(8)
        }
        .frame(width: 180)
    }

    private func textSpriteThumbnail(sprite: TextSprite) -> some View {
        let grid = sprite.pixels
        let gridH = grid.count
        let gridW = grid.first?.count ?? 1

        return Canvas { context, size in
            let scale = min(size.width / CGFloat(gridW), size.height / CGFloat(gridH))
            let offsetX = (size.width - CGFloat(gridW) * scale) / 2
            let offsetY = (size.height - CGFloat(gridH) * scale) / 2

            for y in 0..<gridH {
                for x in 0..<gridW {
                    let c = grid[y][x]
                    guard c != 0 else { continue }
                    let color: Color
                    switch c {
                    case 1: color = .white
                    case 2: color = Color(white: 0.6)
                    default: color = .black
                    }
                    let rect = CGRect(
                        x: offsetX + CGFloat(x) * scale,
                        y: offsetY + CGFloat(y) * scale,
                        width: scale + 0.5,
                        height: scale + 0.5
                    )
                    context.fill(Path(rect), with: .color(color))
                }
            }
        }
        .background(Color.gray.opacity(0.2))
    }
}
