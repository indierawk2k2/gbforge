import SwiftUI

/// Main spell icon editor: 3-panel layout with thumbnail list, pixel canvas, and info panel.
struct SpellEditorView: View {
    @ObservedObject var document: TileDocument

    var body: some View {
        HSplitView {
            // Left sidebar: spell icon thumbnails
            SpellListView(document: document)

            // Center: 8x8 pixel canvas with legend
            VStack(spacing: 12) {
                SpellGridView(document: document)

                // Mouse button legend
                HStack(spacing: 16) {
                    Text("L=0").font(.caption.monospaced())
                    Text("R=1").font(.caption.monospaced())
                    Text("\u{2318}L=2").font(.caption.monospaced())
                    Text("\u{2318}R=3").font(.caption.monospaced())
                }
                .foregroundStyle(.secondary)
            }
            .padding()
            .frame(minWidth: 420)

            // Right panel: info, palette, previews
            SpellInfoPanel(document: document)
        }
    }
}
