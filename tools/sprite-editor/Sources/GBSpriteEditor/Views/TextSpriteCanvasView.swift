import SwiftUI

/// Center panel: text input field + large editable canvas showing composed pixels.
/// Left click = color 0 (transparent), Right click = color 1 (white),
/// Cmd+Left = color 2, Cmd+Right = color 3 (black).
struct TextSpriteCanvasView: View {
    @ObservedObject var document: TileDocument

    private let pixelSize: CGFloat = 8

    private var sprite: TextSprite {
        guard !document.textSprites.isEmpty,
              document.selectedTextSpriteIndex < document.textSprites.count else {
            return TextSprite()
        }
        return document.textSprites[document.selectedTextSpriteIndex]
    }

    private var textBinding: Binding<String> {
        Binding(
            get: { sprite.text },
            set: {
                guard !document.textSprites.isEmpty,
                      document.selectedTextSpriteIndex < document.textSprites.count else { return }
                document.textSprites[document.selectedTextSpriteIndex].text = $0
                document.recomposeTextSprite(at: document.selectedTextSpriteIndex)
            }
        )
    }

    var body: some View {
        VStack(spacing: 12) {
            HStack {
                Text("Text:")
                    .font(.caption)
                TextField("Enter text", text: textBinding)
                    .textFieldStyle(.roundedBorder)
                    .frame(maxWidth: 300)
            }

            composedCanvas

            HStack(spacing: 4) {
                legendItem(label: "L=0", desc: "trans")
                legendItem(label: "R=1", desc: "white")
                legendItem(label: "\u{2318}L=2", desc: "mid")
                legendItem(label: "\u{2318}R=3", desc: "black")
            }
            .font(.caption2)
            .foregroundStyle(.secondary)

            HStack(spacing: 16) {
                VStack(spacing: 2) {
                    Text("1x").font(.caption2).foregroundStyle(.secondary)
                    previewCanvas(scale: 1)
                }
                VStack(spacing: 2) {
                    Text("2x").font(.caption2).foregroundStyle(.secondary)
                    previewCanvas(scale: 2)
                }
                VStack(spacing: 2) {
                    Text("4x").font(.caption2).foregroundStyle(.secondary)
                    previewCanvas(scale: 4)
                }
            }
        }
    }

    private func legendItem(label: String, desc: String) -> some View {
        Text("\(label) \(desc)")
            .padding(.horizontal, 4)
            .padding(.vertical, 1)
            .background(Color.gray.opacity(0.1))
            .cornerRadius(2)
    }

    private var composedCanvas: some View {
        let grid = sprite.pixels
        let gridH = grid.count
        let gridW = grid.first?.count ?? 8
        let canvasW = CGFloat(gridW) * pixelSize
        let canvasH = CGFloat(gridH) * pixelSize

        return ZStack {
            Canvas { context, size in
                // Background
                context.fill(
                    Path(CGRect(origin: .zero, size: size)),
                    with: .color(Color.gray.opacity(0.15))
                )

                // Draw pixels
                for y in 0..<gridH {
                    for x in 0..<gridW {
                        let c = grid[y][x]
                        let color: Color
                        switch c {
                        case 1: color = .white
                        case 2: color = Color(white: 0.6)
                        case 3: color = .black
                        default: continue
                        }
                        let rect = CGRect(
                            x: CGFloat(x) * pixelSize,
                            y: CGFloat(y) * pixelSize,
                            width: pixelSize,
                            height: pixelSize
                        )
                        context.fill(Path(rect), with: .color(color))
                    }
                }

                // 8px tile grid overlay (vertical lines)
                context.stroke(
                    Path { p in
                        for col in stride(from: 0, through: gridW, by: 8) {
                            let x = CGFloat(col) * pixelSize
                            p.move(to: CGPoint(x: x, y: 0))
                            p.addLine(to: CGPoint(x: x, y: canvasH))
                        }
                    },
                    with: .color(.blue.opacity(0.3)),
                    lineWidth: 1
                )

                // Top/bottom half divider (row 8)
                let dividerY = 8 * pixelSize
                context.stroke(
                    Path { p in
                        p.move(to: CGPoint(x: 0, y: dividerY))
                        p.addLine(to: CGPoint(x: canvasW, y: dividerY))
                    },
                    with: .color(.red.opacity(0.5)),
                    lineWidth: 1
                )
            }
            .frame(width: canvasW, height: canvasH)

            SpellMouseOverlay(
                cellSize: pixelSize,
                gridSize: gridW,
                onMouseDown: { col, row, colorIndex, _ in
                    document.handleTextSpriteGridClick(x: col, y: row, colorIndex: colorIndex, isDown: true)
                },
                onMouseDrag: { col, row, colorIndex, _ in
                    document.handleTextSpriteGridClick(x: col, y: row, colorIndex: colorIndex, isDown: false)
                },
                onMouseUp: { _, _ in }
            )
            .frame(width: canvasW, height: canvasH)
        }
        .border(Color.gray.opacity(0.3), width: 1)
    }

    private func previewCanvas(scale: Int) -> some View {
        let grid = sprite.pixels
        let gridH = grid.count
        let gridW = grid.first?.count ?? 8
        let size = CGSize(width: CGFloat(gridW * scale), height: CGFloat(gridH * scale))

        return Canvas { context, canvasSize in
            let px = canvasSize.width / CGFloat(gridW)
            let py = canvasSize.height / CGFloat(gridH)
            for y in 0..<gridH {
                for x in 0..<gridW {
                    let c = grid[y][x]
                    let color: Color
                    switch c {
                    case 1: color = .white
                    case 2: color = Color(white: 0.6)
                    case 3: color = .black
                    default: continue
                    }
                    let rect = CGRect(
                        x: CGFloat(x) * px,
                        y: CGFloat(y) * py,
                        width: px + 0.5,
                        height: py + 0.5
                    )
                    context.fill(Path(rect), with: .color(color))
                }
            }
        }
        .frame(width: size.width, height: size.height)
        .background(Color.gray.opacity(0.3))
    }
}
