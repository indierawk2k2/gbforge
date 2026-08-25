import SwiftUI

@main
struct GBSpriteEditorApp: App {
    @StateObject private var document = TileDocument()

    var body: some Scene {
        WindowGroup {
            ContentView(document: document)
        }
        .defaultSize(width: 900, height: 640)
        .commands {
            CommandGroup(replacing: .undoRedo) {
                Button("Undo") {
                    document.undo()
                }
                .keyboardShortcut("z", modifiers: .command)
                .disabled(!document.canUndo)

                Button("Redo") {
                    document.redo()
                }
                .keyboardShortcut("z", modifiers: [.command, .shift])
                .disabled(!document.canRedo)
            }

            CommandGroup(replacing: .pasteboard) {
                Button("Paste Image") {
                    ClipboardImporter.paste(into: document)
                }
                .keyboardShortcut("v", modifiers: .command)
                .disabled(!ClipboardImporter.hasImage)

                Button("Paste (Match Palette)") {
                    ClipboardImporter.pasteMatchingPalette(into: document)
                }
                .keyboardShortcut("v", modifiers: [.command, .option])
                .disabled(!ClipboardImporter.hasImage)
            }
        }
    }
}
