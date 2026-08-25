import SwiftUI

/// Main text sprite editor: 3-panel layout matching SpellEditorView.
struct TextEditorView: View {
    @ObservedObject var document: TileDocument

    var body: some View {
        HSplitView {
            TextSpriteListView(document: document)

            TextSpriteCanvasView(document: document)
                .padding()
                .frame(minWidth: 440)

            TextSpriteInfoPanel(document: document)
        }
    }
}
