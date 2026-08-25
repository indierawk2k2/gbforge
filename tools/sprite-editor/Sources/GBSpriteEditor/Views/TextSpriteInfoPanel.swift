import SwiftUI

/// Right panel: name, tile info, export button.
struct TextSpriteInfoPanel: View {
    @ObservedObject var document: TileDocument
    @State private var showingExporter = false
    @State private var exportedCode = ""

    private var hasSprite: Bool {
        !document.textSprites.isEmpty && document.selectedTextSpriteIndex < document.textSprites.count
    }

    private var nameBinding: Binding<String> {
        Binding(
            get: { hasSprite ? document.textSprites[document.selectedTextSpriteIndex].name : "" },
            set: { if hasSprite { document.textSprites[document.selectedTextSpriteIndex].name = $0 } }
        )
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            if hasSprite {
                let sprite = document.textSprites[document.selectedTextSpriteIndex]

                VStack(alignment: .leading, spacing: 4) {
                    Text("Name")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    TextField("Name", text: nameBinding)
                        .textFieldStyle(.roundedBorder)
                }

                Divider()

                VStack(alignment: .leading, spacing: 4) {
                    let cols = sprite.columnCount
                    let tiles = cols * 2
                    Text("Tile Info")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Text("\(cols) columns x 2 halves = \(tiles) tiles")
                        .font(.caption.monospaced())
                    Text("\(tiles * 16) bytes")
                        .font(.caption.monospaced())
                        .foregroundStyle(.secondary)
                }

                Divider()

                VStack(alignment: .leading, spacing: 4) {
                    Text("2bpp Colors")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    HStack(spacing: 4) {
                        colorSwatch(label: "0=Trans", bgColor: .clear)
                        colorSwatch(label: "1=White", bgColor: .white)
                        colorSwatch(label: "2=Mid", bgColor: Color(white: 0.6))
                        colorSwatch(label: "3=Black", bgColor: .black)
                    }
                }

                Spacer()

                Button("Export C Code...") {
                    exportedCode = CExporter.generateTextSpriteC(sprite: sprite)
                    showingExporter = true
                }
                .frame(maxWidth: .infinity)
            } else {
                Text("No text sprites")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Spacer()
            }
        }
        .padding()
        .frame(width: 200)
        .sheet(isPresented: $showingExporter) {
            ExportSheet(code: exportedCode, isPresented: $showingExporter)
        }
    }

    private func colorSwatch(label: String, bgColor: Color) -> some View {
        VStack(spacing: 1) {
            Rectangle()
                .fill(bgColor)
                .frame(width: 20, height: 20)
                .border(Color.gray.opacity(0.5), width: 1)
            Text(label)
                .font(.system(size: 8))
                .foregroundStyle(.secondary)
        }
    }
}
