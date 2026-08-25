import SwiftUI
import UniformTypeIdentifiers

/// Persistent toolbar row above the tab picker with all file operations.
/// Visible from both Tiles and Spells tabs.
struct GlobalToolbar: View {
    @ObservedObject var document: TileDocument
    @Binding var errorMessage: String?
    var onImportPNG: () -> Void
    var onExportCCode: () -> Void

    var body: some View {
        HStack(spacing: 8) {
            // File operations
            Button { loadFile() } label: {
                Label("Open", systemImage: "folder")
            }
            .help("Open .gbtiles file")

            Button { saveFile() } label: {
                Label("Save", systemImage: "square.and.arrow.down")
            }
            .help("Save")
            .disabled(document.currentFileURL == nil)

            Button { saveFileAs() } label: {
                Label("Save As", systemImage: "square.and.arrow.down.on.square")
            }
            .help("Save As")

            Divider().frame(height: 16)

            // Game directory operations
            Button { pickOutputDirectory() } label: {
                Label("Set Output Dir", systemImage: "folder.badge.gearshape")
            }
            .help("Set output directory for C export")

            Button { loadFromGameDirectory() } label: {
                Label("Load Game Dir", systemImage: "arrow.down.doc")
            }
            .help("Load tiles from game directory C files")
            .disabled(document.outputDirectory == nil)

            Button { saveToGameDirectory() } label: {
                Label("Save to Game Dir", systemImage: "gamecontroller")
            }
            .help("Export to game directory")
            .disabled(document.outputDirectory == nil)

            Divider().frame(height: 16)

            // Clipboard operations
            Button {
                ClipboardImporter.paste(into: document)
            } label: {
                Label("Paste", systemImage: "doc.on.clipboard")
            }
            .help("Paste Image (⌘V)")
            .disabled(!ClipboardImporter.hasImage)

            Button {
                ClipboardImporter.pasteMatchingPalette(into: document)
            } label: {
                Label("Smart Paste", systemImage: "paintpalette")
            }
            .help("Paste matching existing palette (⌥⌘V)")
            .disabled(!ClipboardImporter.hasImage)

            Button { onImportPNG() } label: {
                Label("Import PNG", systemImage: "photo")
            }
            .help("Import PNG image")

            Button { onExportCCode() } label: {
                Label("Export C", systemImage: "chevron.left.forwardslash.chevron.right")
            }
            .help("Export C code")

            Spacer()

            // File info
            fileInfoSection
        }
        .labelStyle(.iconOnly)
        .buttonStyle(.bordered)
        .controlSize(.small)
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .frame(maxWidth: .infinity)
        .background(.bar)
    }

    // MARK: - File info display

    @ViewBuilder
    private var fileInfoSection: some View {
        VStack(alignment: .trailing, spacing: 1) {
            if let url = document.currentFileURL {
                Text(url.lastPathComponent)
                    .font(.caption.monospaced())
                    .lineLimit(1)
                    .truncationMode(.middle)
            } else {
                Text("No file")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            if let dir = document.outputDirectory {
                Text("Out: \(dir.lastPathComponent)/")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                    .truncationMode(.middle)
            }
        }
    }

    // MARK: - File operations

    private func loadFile() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [UTType(filenameExtension: "gbtiles") ?? .data]
        panel.allowsMultipleSelection = false
        guard panel.runModal() == .OK, let url = panel.url else { return }
        do {
            try document.loadFromFile(url)
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    private func saveFile() {
        do {
            try document.saveToFile()
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    private func saveFileAs() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [UTType(filenameExtension: "gbtiles") ?? .data]
        panel.nameFieldStringValue = document.currentFileURL?.lastPathComponent ?? "tiles.gbtiles"
        guard panel.runModal() == .OK, let url = panel.url else { return }
        do {
            try document.saveToFile(url)
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    private func loadFromGameDirectory() {
        guard let dir = document.outputDirectory else {
            errorMessage = "Please set an output directory first."
            return
        }
        do {
            try document.loadFromGameDirectory(dir)
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    private func saveToGameDirectory() {
        guard let dir = document.outputDirectory else {
            errorMessage = "Please set an output directory first."
            return
        }
        do {
            try CExporter.exportToGameDirectory(document: document, directory: dir)
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    private func pickOutputDirectory() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        panel.prompt = "Set Output Directory"
        if let current = document.outputDirectory {
            panel.directoryURL = current
        }
        guard panel.runModal() == .OK, let url = panel.url else { return }
        document.outputDirectory = url
    }
}
