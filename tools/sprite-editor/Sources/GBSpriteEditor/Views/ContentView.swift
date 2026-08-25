import SwiftUI
import UniformTypeIdentifiers

/// Main app layout: tile list sidebar, pixel editor center, palette + export right.
struct ContentView: View {
    @ObservedObject var document: TileDocument
    @State private var showingImporter = false
    @State private var showingExporter = false
    @State private var exportedCode = ""
    @State private var errorMessage: String?
    @State private var selectedTab = 0

    var body: some View {
        VStack(spacing: 0) {
            // Tab bar — fixed at top with its own background
            Picker("Editor", selection: $selectedTab) {
                Text("Tiles").tag(0)
                Text("Spells").tag(1)
                Text("Text").tag(2)
            }
            .pickerStyle(.segmented)
            .padding(.horizontal, 12)
            .padding(.vertical, 8)
            .background(.bar)

            Divider()

            // Tab content — fills remaining space below the tab bar
            if selectedTab == 0 {
                tileEditorBody
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .clipped()
            } else if selectedTab == 1 {
                SpellEditorView(document: document)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .clipped()
            } else if selectedTab == 2 {
                TextEditorView(document: document)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .clipped()
            }
        }
        .fileImporter(
            isPresented: $showingImporter,
            allowedContentTypes: [.png],
            allowsMultipleSelection: false
        ) { result in
            if case .success(let urls) = result, let url = urls.first {
                PNGImporter.importPNG(url: url, into: document)
            }
        }
        .sheet(isPresented: $showingExporter) {
            ExportSheet(code: exportedCode, isPresented: $showingExporter)
        }
        .alert("Error", isPresented: showingError, actions: {
            Button("OK") { errorMessage = nil }
        }, message: {
            Text(errorMessage ?? "Unknown error")
        })
    }

    private var tileEditorBody: some View {
        HSplitView {
            // Left sidebar: tile list
            TileListView(document: document)

            // Center: pixel editor (16x16 tile or 8x8 indicator)
            if let indicatorIdx = document.selectedIndicatorIndex {
                indicatorEditorBody(index: indicatorIdx)
                    .padding()
                    .frame(minWidth: 440)
            } else {
                VStack(spacing: 12) {
                    TextField("Tile Name", text: tileNameBinding)
                        .textFieldStyle(.roundedBorder)
                        .frame(width: 200)

                    TileGridView(document: document)

                    // Mouse button legend
                    HStack(spacing: 16) {
                        Text("L=0").font(.caption.monospaced())
                        Text("R=1").font(.caption.monospaced())
                        Text("\u{2318}L=2").font(.caption.monospaced())
                        Text("\u{2318}R=3").font(.caption.monospaced())
                    }
                    .foregroundStyle(.secondary)

                    // Small actual-size previews
                    HStack(spacing: 16) {
                        Text("1x")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        tilePreviewCanvas(document: document)
                            .frame(width: 16, height: 16)
                        Text("2x")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        tilePreviewCanvas(document: document)
                            .frame(width: 32, height: 32)
                        Text("4x")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        tilePreviewCanvas(document: document)
                            .frame(width: 64, height: 64)
                    }
                }
                .padding()
                .frame(minWidth: 440)
            }

            // Right panel: palette + import/export
            VStack(spacing: 0) {
                PaletteView(document: document)

                Divider()

                VStack(spacing: 8) {
                    // File info
                    fileInfoSection

                    Divider()

                    // File operations
                    HStack(spacing: 4) {
                        Button("Load...") { loadFile() }
                        Button("Save") { saveFile() }
                            .disabled(document.currentFileURL == nil)
                        Button("Save As...") { saveFileAs() }
                    }
                    .frame(maxWidth: .infinity)

                    Button("Set Output Dir...") { pickOutputDirectory() }
                        .frame(maxWidth: .infinity)

                    Button("Load from Game Dir") { loadFromGameDirectory() }
                        .frame(maxWidth: .infinity)
                        .disabled(document.outputDirectory == nil)

                    Button("Save to Game Dir") { saveToGameDirectory() }
                        .frame(maxWidth: .infinity)
                        .disabled(document.outputDirectory == nil)

                    Divider()

                    Button("Paste Image (\u{2318}V)") {
                        ClipboardImporter.paste(into: document)
                    }
                    .frame(maxWidth: .infinity)
                    .disabled(!ClipboardImporter.hasImage)

                    Button("Paste (Match Palette) \u{2325}\u{2318}V") {
                        ClipboardImporter.pasteMatchingPalette(into: document)
                    }
                    .frame(maxWidth: .infinity)
                    .disabled(!ClipboardImporter.hasImage)

                    Button("Import PNG...") {
                        showingImporter = true
                    }
                    .frame(maxWidth: .infinity)

                    Button("Export C Code...") {
                        exportedCode = CExporter.export(document: document)
                        showingExporter = true
                    }
                    .frame(maxWidth: .infinity)
                }
                .padding()
            }
            .frame(width: 260)
        }
    }

    private var showingError: Binding<Bool> {
        Binding(get: { errorMessage != nil }, set: { if !$0 { errorMessage = nil } })
    }

    @ViewBuilder
    private var fileInfoSection: some View {
        VStack(alignment: .leading, spacing: 2) {
            if let url = document.currentFileURL {
                Text(url.lastPathComponent)
                    .font(.caption.monospaced())
                    .lineLimit(1)
                    .truncationMode(.middle)
            } else {
                Text("No file loaded")
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

    private var tileNameBinding: Binding<String> {
        Binding(
            get: { document.selectedTile.name },
            set: { document.tiles[document.selectedTileIndex].name = $0 }
        )
    }

    // MARK: - Indicator editor

    @ViewBuilder
    private func indicatorEditorBody(index: Int) -> some View {
        let kind = IndicatorKind(rawValue: index)!
        let tile = document.indicatorTiles.tiles[index]
        let palette = document.palettes[kind.paletteIndex]
        let cellSize: CGFloat = 40
        let gridSize = 8
        let totalSize = cellSize * CGFloat(gridSize)
        let c0 = palette[0].color
        let c1 = palette[1].color
        let c2 = palette[2].color
        let c3 = palette[3].color

        VStack(spacing: 12) {
            Text("\(kind.name) Indicator")
                .font(.headline)

            ZStack {
                Canvas { context, size in
                    let colors = [c0, c1, c2, c3]

                    // Draw pixel cells
                    for row in 0..<gridSize {
                        for col in 0..<gridSize {
                            let colorIndex = Int(tile.pixels[row][col])
                            let rect = CGRect(
                                x: CGFloat(col) * cellSize,
                                y: CGFloat(row) * cellSize,
                                width: cellSize,
                                height: cellSize
                            )
                            context.fill(Path(rect), with: .color(colors[colorIndex]))
                        }
                    }

                    // Draw floating pixels at their current origin
                    if let pixels = document.floatingPixels8x8, let origin = document.floatingOrigin8x8 {
                        for dy in 0..<pixels.count {
                            for dx in 0..<pixels[dy].count {
                                let px = origin.x + dx
                                let py = origin.y + dy
                                guard px >= 0, px < gridSize, py >= 0, py < gridSize else { continue }
                                let rect = CGRect(
                                    x: CGFloat(px) * cellSize,
                                    y: CGFloat(py) * cellSize,
                                    width: cellSize,
                                    height: cellSize
                                )
                                context.fill(Path(rect), with: .color(colors[Int(pixels[dy][dx])]))
                            }
                        }
                    }

                    // Grid lines
                    let lines = Path { path in
                        for i in 1..<gridSize {
                            let pos = CGFloat(i) * cellSize
                            path.move(to: CGPoint(x: pos, y: 0))
                            path.addLine(to: CGPoint(x: pos, y: totalSize))
                            path.move(to: CGPoint(x: 0, y: pos))
                            path.addLine(to: CGPoint(x: totalSize, y: pos))
                        }
                    }
                    context.stroke(lines, with: .color(.gray.opacity(0.3)), lineWidth: 0.5)

                    let border = Path(CGRect(x: 0, y: 0, width: totalSize, height: totalSize))
                    context.stroke(border, with: .color(.gray), lineWidth: 1)

                    // Draw selection rectangle
                    if let sel = document.selection8x8 {
                        let selRect = CGRect(
                            x: CGFloat(sel.x) * cellSize,
                            y: CGFloat(sel.y) * cellSize,
                            width: CGFloat(sel.w) * cellSize,
                            height: CGFloat(sel.h) * cellSize
                        )
                        let dashStyle = StrokeStyle(lineWidth: 2, dash: [6, 3])
                        context.stroke(Path(selRect), with: .color(.accentColor), style: dashStyle)
                    }

                    // Draw floating pixels outline
                    if let pixels = document.floatingPixels8x8, let origin = document.floatingOrigin8x8 {
                        let floatRect = CGRect(
                            x: CGFloat(origin.x) * cellSize,
                            y: CGFloat(origin.y) * cellSize,
                            width: CGFloat(pixels[0].count) * cellSize,
                            height: CGFloat(pixels.count) * cellSize
                        )
                        let dashStyle = StrokeStyle(lineWidth: 2, dash: [4, 2])
                        context.stroke(Path(floatRect), with: .color(.orange), style: dashStyle)
                    }
                }
                .id("indicator-\(index)")
                .frame(width: totalSize, height: totalSize)

                SpellMouseOverlay(
                    cellSize: cellSize,
                    gridSize: gridSize,
                    onMouseDown: { col, row, colorIndex, hasShift in
                        guard col >= 0, col < gridSize, row >= 0, row < gridSize else { return }

                        if hasShift {
                            if document.floatingPixels8x8 != nil {
                                document.commitIndicatorFloating()
                            }
                            document.selectionAnchor8x8 = (x: col, y: row)
                            document.selection8x8 = (x: col, y: row, w: 1, h: 1)
                            return
                        }

                        if let sel = document.selection8x8, document.floatingPixels8x8 == nil {
                            if col >= sel.x && col < sel.x + sel.w &&
                               row >= sel.y && row < sel.y + sel.h {
                                document.liftIndicatorSelection()
                                document.moveAnchor8x8 = (x: col, y: row)
                                return
                            }
                        }

                        document.clearIndicatorSelection()
                        document.handleIndicatorGridClick(x: col, y: row, colorIndex: colorIndex, isDown: true)
                    },
                    onMouseDrag: { col, row, colorIndex, hasShift in
                        if hasShift, let anchor = document.selectionAnchor8x8 {
                            let clampedCol = max(0, min(gridSize - 1, col))
                            let clampedRow = max(0, min(gridSize - 1, row))
                            let minX = min(anchor.x, clampedCol)
                            let maxX = max(anchor.x, clampedCol)
                            let minY = min(anchor.y, clampedRow)
                            let maxY = max(anchor.y, clampedRow)
                            document.selection8x8 = (x: minX, y: minY, w: maxX - minX + 1, h: maxY - minY + 1)
                            return
                        }

                        if document.floatingPixels8x8 != nil, let anchor = document.moveAnchor8x8, let origin = document.floatingOrigin8x8 {
                            let dx = col - anchor.x
                            let dy = row - anchor.y
                            document.floatingOrigin8x8 = (x: origin.x + dx, y: origin.y + dy)
                            document.moveAnchor8x8 = (x: col, y: row)
                            return
                        }

                        guard col >= 0, col < gridSize, row >= 0, row < gridSize else { return }
                        if document.selection8x8 == nil {
                            document.handleIndicatorGridClick(x: col, y: row, colorIndex: colorIndex, isDown: false)
                        }
                    },
                    onMouseUp: { _, _ in
                        if document.selectionAnchor8x8 != nil {
                            document.selectionAnchor8x8 = nil
                            return
                        }

                        if document.floatingPixels8x8 != nil {
                            document.commitIndicatorFloating()
                            return
                        }
                    }
                )
                .frame(width: totalSize, height: totalSize)
            }

            // Mouse button legend
            HStack(spacing: 16) {
                Text("L=0").font(.caption.monospaced())
                Text("R=1").font(.caption.monospaced())
                Text("\u{2318}L=2").font(.caption.monospaced())
                Text("\u{2318}R=3").font(.caption.monospaced())
            }
            .foregroundStyle(.secondary)

            // Previews
            HStack(spacing: 16) {
                VStack(spacing: 2) {
                    Text("1x").font(.caption2).foregroundStyle(.secondary)
                    indicatorPreview(index: index, palette: palette)
                        .frame(width: 8, height: 8)
                }
                VStack(spacing: 2) {
                    Text("2x").font(.caption2).foregroundStyle(.secondary)
                    indicatorPreview(index: index, palette: palette)
                        .frame(width: 16, height: 16)
                }
                VStack(spacing: 2) {
                    Text("4x").font(.caption2).foregroundStyle(.secondary)
                    indicatorPreview(index: index, palette: palette)
                        .frame(width: 32, height: 32)
                }
            }
        }
    }

    private func indicatorPreview(index: Int, palette: GBPalette) -> some View {
        let tile = document.indicatorTiles.tiles[index]
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

/// Renders a tile preview at any size.
func tilePreviewCanvas(document: TileDocument) -> some View {
    let tile = document.selectedTile
    let palette = document.selectedPalette
    return Canvas { context, size in
        let px = size.width / 16
        let py = size.height / 16
        for row in 0..<16 {
            for col in 0..<16 {
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

/// Sheet for displaying and copying exported C code.
struct ExportSheet: View {
    let code: String
    @Binding var isPresented: Bool
    @State private var copied = false

    var body: some View {
        VStack(spacing: 12) {
            HStack {
                Text("Exported C Code")
                    .font(.headline)
                Spacer()
                Button(copied ? "Copied" : "Copy") {
                    NSPasteboard.general.clearContents()
                    NSPasteboard.general.setString(code, forType: .string)
                    copied = true
                }
                Button("Save...") {
                    saveToFile()
                }
                Button("Close") {
                    isPresented = false
                }
            }

            ScrollView {
                Text(code)
                    .font(.system(.caption, design: .monospaced))
                    .textSelection(.enabled)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(8)
            }
            .background(Color(nsColor: .textBackgroundColor))
            .cornerRadius(4)
        }
        .padding()
        .frame(width: 700, height: 500)
    }

    private func saveToFile() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.cSource]
        panel.nameFieldStringValue = "tiles_data.c"
        if panel.runModal() == .OK, let url = panel.url {
            try? code.write(to: url, atomically: true, encoding: .utf8)
        }
    }
}
