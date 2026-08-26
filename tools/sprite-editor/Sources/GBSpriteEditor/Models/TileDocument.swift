import SwiftUI

/// The root document model: a set of tiles with their palettes.
class TileDocument: ObservableObject {
    @Published var tiles: [GBTile16x16]
    @Published var palettes: [GBPalette]
    @Published var selectedTileIndex: Int = 0
    @Published var selectedColorIndex: Int = 3  // Default to darkest color for painting
    @Published var spellIcons = SpellIconSet()
    @Published var spellCastTiles = SpellCastTileSet()
    @Published var selectedSpellIndex: Int = 0
    @Published var indicatorTiles = IndicatorTileSet()
    @Published var selectedIndicatorIndex: Int? = nil

    // MARK: - Text sprites (session-only, not persisted to file)

    @Published var textSprites: [TextSprite] = []
    @Published var selectedTextSpriteIndex: Int = 0

    // MARK: - Selection / floating state

    /// Selection rectangle (nil = no selection)
    @Published var selection: (x: Int, y: Int, w: Int, h: Int)?

    /// Anchor point during selection drag (nil = not actively selecting)
    @Published var selectionAnchor: (x: Int, y: Int)?

    /// Floating pixels lifted during a move (nil = not moving)
    @Published var floatingPixels: [[UInt8]]?
    @Published var floatingOrigin: (x: Int, y: Int)?

    /// Anchor point for computing move delta
    @Published var moveAnchor: (x: Int, y: Int)?

    // MARK: - 8x8 selection / floating state (spell icons & indicator tiles)

    @Published var selection8x8: (x: Int, y: Int, w: Int, h: Int)?
    @Published var selectionAnchor8x8: (x: Int, y: Int)?
    @Published var floatingPixels8x8: [[UInt8]]?
    @Published var floatingOrigin8x8: (x: Int, y: Int)?
    @Published var moveAnchor8x8: (x: Int, y: Int)?

    /// The URL of the currently loaded .gbtiles file (nil if new/unsaved).
    @Published var currentFileURL: URL?
    /// The output directory for the exported C files (the game's res/).
    @Published var outputDirectory: URL?

    // MARK: - Undo / Redo

    /// Whether the selected spell index refers to a 32x32 cast tile (indices 14-20).
    var isSpellCastSelected: Bool { selectedSpellIndex >= 14 }

    /// The SpellKind for the selected cast tile, or nil if an icon is selected.
    var selectedCastSpellKind: SpellKind? {
        guard isSpellCastSelected else { return nil }
        return SpellKind(rawValue: selectedSpellIndex - 14)
    }

    /// Snapshot of the restorable document state.
    struct DocumentSnapshot {
        let tiles: [GBTile16x16]
        let palettes: [GBPalette]
        let spellIcons: SpellIconSet
        let spellCastTiles: SpellCastTileSet
        let indicatorTiles: IndicatorTileSet
        let textSprites: [TextSprite]
    }

    /// Categories of undo actions for coalescing palette edits.
    enum UndoActionKind {
        case paint
        case paletteColor
        case paletteComponent
        case paletteIndex
        case liftSelection
        case commitFloating
        case paste
        case pasteMatchPalette
        case pngImport
    }

    private static let maxUndoLevels = 64

    private var undoStack: [DocumentSnapshot] = []
    private var redoStack: [DocumentSnapshot] = []

    /// Tracks the last undo push time and action kind for coalescing.
    private var lastUndoPushTime: Date = .distantPast
    private var lastUndoActionKind: UndoActionKind?

    /// Whether undo/redo are currently available (for menu item validation).
    @Published var canUndo: Bool = false
    @Published var canRedo: Bool = false

    /// Push the current state onto the undo stack before a mutation.
    /// For `.paletteColor` and `.paletteComponent` actions, coalesces pushes
    /// that occur within 0.5 seconds of the same action kind.
    func pushUndo(actionKind: UndoActionKind = .paint) {
        let now = Date()

        // Coalesce rapid palette slider edits of the same kind
        if actionKind == .paletteColor || actionKind == .paletteComponent {
            if lastUndoActionKind == actionKind,
               now.timeIntervalSince(lastUndoPushTime) < 0.5 {
                return
            }
        }

        let snapshot = DocumentSnapshot(tiles: tiles, palettes: palettes, spellIcons: spellIcons, spellCastTiles: spellCastTiles, indicatorTiles: indicatorTiles, textSprites: textSprites)
        undoStack.append(snapshot)
        if undoStack.count > Self.maxUndoLevels {
            undoStack.removeFirst()
        }
        // Any new action invalidates the redo history
        redoStack.removeAll()

        lastUndoPushTime = now
        lastUndoActionKind = actionKind
        canUndo = true
        canRedo = false
    }

    /// Public entry point for external callers (e.g. ClipboardImporter, PNGImporter)
    /// to push an undo snapshot before a batch of mutations.
    func beginUndoGroup(actionKind: UndoActionKind = .paste) {
        pushUndo(actionKind: actionKind)
    }

    /// Restore the most recent undo snapshot.
    func undo() {
        guard let snapshot = undoStack.popLast() else { return }
        // Save current state to redo stack
        let current = DocumentSnapshot(tiles: tiles, palettes: palettes, spellIcons: spellIcons, spellCastTiles: spellCastTiles, indicatorTiles: indicatorTiles, textSprites: textSprites)
        redoStack.append(current)

        tiles = snapshot.tiles
        palettes = snapshot.palettes
        spellIcons = snapshot.spellIcons
        spellCastTiles = snapshot.spellCastTiles
        indicatorTiles = snapshot.indicatorTiles
        textSprites = snapshot.textSprites
        // Clamp selectedTileIndex in case tile count changed
        if selectedTileIndex >= tiles.count {
            selectedTileIndex = max(0, tiles.count - 1)
        }
        // Clear any floating state since undo restores tile data directly
        floatingPixels = nil
        floatingOrigin = nil
        moveAnchor = nil
        selection = nil
        selectionAnchor = nil

        canUndo = !undoStack.isEmpty
        canRedo = true
    }

    /// Re-apply the most recently undone change.
    func redo() {
        guard let snapshot = redoStack.popLast() else { return }
        // Save current state to undo stack
        let current = DocumentSnapshot(tiles: tiles, palettes: palettes, spellIcons: spellIcons, spellCastTiles: spellCastTiles, indicatorTiles: indicatorTiles, textSprites: textSprites)
        undoStack.append(current)

        tiles = snapshot.tiles
        palettes = snapshot.palettes
        spellIcons = snapshot.spellIcons
        spellCastTiles = snapshot.spellCastTiles
        indicatorTiles = snapshot.indicatorTiles
        textSprites = snapshot.textSprites
        if selectedTileIndex >= tiles.count {
            selectedTileIndex = max(0, tiles.count - 1)
        }
        floatingPixels = nil
        floatingOrigin = nil
        moveAnchor = nil
        selection = nil
        selectionAnchor = nil

        canUndo = true
        canRedo = !redoStack.isEmpty
    }

    /// The game whose res/ this editor opens by default. A repo with
    /// several games points this at whichever one is being worked on.
    static let defaultGameResPath = "examples/cascadia/res"

    /// Well-known path for the shared .gbtiles file, relative to the app bundle's parent.
    static let defaultFileRelativePath = "../\(defaultGameResPath)/tiles.gbtiles"
    static let defaultOutputRelativePath = "../\(defaultGameResPath)"

    /// Resolve paths to the game's res/ using the compile-time source location.
    /// #filePath is immune to macOS app translocation (which relocates .app bundles
    /// to a temp path on launch, breaking Bundle.main.bundleURL-relative paths).
    private static func resolveGamePaths() -> (fileURL: URL, outDir: URL) {
        // #filePath → .../tools/sprite-editor/Sources/GBSpriteEditor/Models/TileDocument.swift
        // Walk up to the repo root (6 levels), then into the game's res/
        let sourceFile = URL(fileURLWithPath: #filePath)
        let repoRoot = sourceFile
            .deletingLastPathComponent()  // Models/
            .deletingLastPathComponent()  // GBSpriteEditor/
            .deletingLastPathComponent()  // Sources/
            .deletingLastPathComponent()  // sprite-editor/
            .deletingLastPathComponent()  // tools/
            .deletingLastPathComponent()  // repo root
        let outDir = repoRoot.appendingPathComponent(defaultGameResPath).standardized
        let fileURL = outDir.appendingPathComponent("tiles.gbtiles")
        return (fileURL, outDir)
    }

    init() {
        self.palettes = Self.defaultPalettes()
        self.tiles = Self.defaultTiles()

        let paths = Self.resolveGamePaths()
        let fileURL = paths.fileURL
        let outDir = paths.outDir

        // Auto-load from game C files (source of truth), fall back to .gbtiles
        let tilesC = outDir.appendingPathComponent("tiles_data.c")
        let palettesC = outDir.appendingPathComponent("palettes.c")

        var loaded = false
        if FileManager.default.fileExists(atPath: tilesC.path),
           FileManager.default.fileExists(atPath: palettesC.path) {
            do {
                try loadFromGameDirectory(outDir)
                loaded = true
            } catch {
                print("Warning: failed to load C files from \(outDir.path): \(error)")
            }
        }
        // Fall back to the last saved .gbtiles rather than the built-in defaults,
        // so a malformed C file doesn't look like the artwork vanished.
        if !loaded, FileManager.default.fileExists(atPath: fileURL.path) {
            do {
                try loadFromFile(fileURL)
            } catch {
                print("Warning: failed to load .gbtiles from \(fileURL.path): \(error)")
            }
        }
        self.outputDirectory = outDir
    }

    var selectedTile: GBTile16x16 {
        get { tiles[selectedTileIndex] }
        set { tiles[selectedTileIndex] = newValue }
    }

    /// The palette for the selected tile.
    var selectedPalette: GBPalette {
        palettes[selectedTile.paletteIndex]
    }

    func addTile() {
        let index = tiles.count
        tiles.append(GBTile16x16(name: "Tile \(index)", paletteIndex: 0))
        selectedTileIndex = tiles.count - 1
    }

    func deleteTile(at index: Int) {
        guard tiles.count > 1 else { return }
        tiles.remove(at: index)
        if selectedTileIndex >= tiles.count {
            selectedTileIndex = tiles.count - 1
        }
    }

    // MARK: - Text sprite CRUD

    func addTextSprite() {
        var sprite = TextSprite(name: "Text \(textSprites.count)", text: "CHAIN")
        sprite.compose(font: PixelFont.shared)
        textSprites.append(sprite)
        selectedTextSpriteIndex = textSprites.count - 1
    }

    func deleteTextSprite(at index: Int) {
        guard !textSprites.isEmpty, index < textSprites.count else { return }
        textSprites.remove(at: index)
        if selectedTextSpriteIndex >= textSprites.count {
            selectedTextSpriteIndex = max(0, textSprites.count - 1)
        }
    }

    /// Handle a text sprite canvas click — paints a pixel on the stored grid.
    func handleTextSpriteGridClick(x: Int, y: Int, colorIndex: Int, isDown: Bool) {
        guard !textSprites.isEmpty,
              selectedTextSpriteIndex < textSprites.count else { return }
        let sprite = textSprites[selectedTextSpriteIndex]
        guard y >= 0, y < sprite.pixels.count,
              x >= 0, x < (sprite.pixels.first?.count ?? 0) else { return }
        if isDown {
            pushUndo(actionKind: .paint)
        }
        textSprites[selectedTextSpriteIndex].pixels[y][x] = UInt8(colorIndex)
    }

    /// Re-compose a text sprite from its text using the font. Called when text changes.
    func recomposeTextSprite(at index: Int) {
        guard index >= 0, index < textSprites.count else { return }
        pushUndo(actionKind: .paint)
        textSprites[index].compose(font: PixelFont.shared)
    }

    /// Handle a grid click — always paints immediately.
    func handleGridClick(x: Int, y: Int, colorIndex: Int, isDown: Bool) {
        if isDown {
            pushUndo(actionKind: .paint)
        }
        tiles[selectedTileIndex].setPixel(x: x, y: y, value: UInt8(colorIndex))
    }

    /// Handle a spell icon grid click — paints a pixel on the selected 8x8 spell icon.
    func handleSpellGridClick(x: Int, y: Int, colorIndex: Int, isDown: Bool) {
        guard x >= 0, x < 8, y >= 0, y < 8 else { return }
        if isDown {
            pushUndo(actionKind: .paint)
        }
        spellIcons.icons[selectedSpellIndex].pixels[y][x] = UInt8(colorIndex)
    }

    /// Handle an indicator tile grid click — paints a pixel on the selected 8x8 indicator tile.
    func handleIndicatorGridClick(x: Int, y: Int, colorIndex: Int, isDown: Bool) {
        guard let idx = selectedIndicatorIndex,
              x >= 0, x < 8, y >= 0, y < 8 else { return }
        if isDown {
            pushUndo(actionKind: .paint)
        }
        indicatorTiles.tiles[idx].pixels[y][x] = UInt8(colorIndex)
    }

    // MARK: - 8x8 selection operations (spell icons & indicator tiles)

    /// Lift pixels from the selected spell icon into 8x8 floating state.
    func liftSpellSelection() {
        guard let sel = selection8x8 else { return }
        pushUndo(actionKind: .liftSelection)
        var pixels = [[UInt8]](repeating: [UInt8](repeating: 0, count: sel.w), count: sel.h)
        for dy in 0..<sel.h {
            for dx in 0..<sel.w {
                let px = sel.x + dx
                let py = sel.y + dy
                guard px >= 0, px < 8, py >= 0, py < 8 else { continue }
                pixels[dy][dx] = spellIcons.icons[selectedSpellIndex].pixels[py][px]
                spellIcons.icons[selectedSpellIndex].pixels[py][px] = 0
            }
        }
        floatingPixels8x8 = pixels
        floatingOrigin8x8 = (x: sel.x, y: sel.y)
    }

    /// Stamp 8x8 floating pixels back onto the selected spell icon.
    func commitSpellFloating() {
        pushUndo(actionKind: .commitFloating)
        if let pixels = floatingPixels8x8, let origin = floatingOrigin8x8 {
            for dy in 0..<pixels.count {
                for dx in 0..<pixels[dy].count {
                    let px = origin.x + dx
                    let py = origin.y + dy
                    guard px >= 0, px < 8, py >= 0, py < 8 else { continue }
                    spellIcons.icons[selectedSpellIndex].pixels[py][px] = pixels[dy][dx]
                }
            }
        }
        floatingPixels8x8 = nil
        floatingOrigin8x8 = nil
        moveAnchor8x8 = nil
        selection8x8 = nil
    }

    func clearSpellSelection() {
        if floatingPixels8x8 != nil {
            commitSpellFloating()
        }
        selection8x8 = nil
        selectionAnchor8x8 = nil
    }

    // MARK: - 32x32 spell cast tile operations

    /// Handle a spell cast grid click — paints a pixel on the selected 32x32 cast tile.
    func handleSpellCastGridClick(x: Int, y: Int, colorIndex: Int, isDown: Bool) {
        guard isSpellCastSelected else { return }
        let castIndex = selectedSpellIndex - 14
        guard x >= 0, x < 32, y >= 0, y < 32 else { return }
        if isDown {
            pushUndo(actionKind: .paint)
        }
        spellCastTiles.tiles[castIndex].setPixel(x: x, y: y, value: UInt8(colorIndex))
    }

    /// Lift pixels from the selected cast tile into floating state.
    func liftSpellCastSelection() {
        guard let sel = selection8x8, isSpellCastSelected else { return }
        let castIndex = selectedSpellIndex - 14
        pushUndo(actionKind: .liftSelection)
        var pixels = [[UInt8]](repeating: [UInt8](repeating: 0, count: sel.w), count: sel.h)
        for dy in 0..<sel.h {
            for dx in 0..<sel.w {
                let px = sel.x + dx
                let py = sel.y + dy
                guard px >= 0, px < 32, py >= 0, py < 32 else { continue }
                pixels[dy][dx] = spellCastTiles.tiles[castIndex].pixel(x: px, y: py)
                spellCastTiles.tiles[castIndex].setPixel(x: px, y: py, value: 0)
            }
        }
        floatingPixels8x8 = pixels
        floatingOrigin8x8 = (x: sel.x, y: sel.y)
    }

    /// Stamp floating pixels back onto the selected cast tile.
    func commitSpellCastFloating() {
        guard isSpellCastSelected else { return }
        let castIndex = selectedSpellIndex - 14
        pushUndo(actionKind: .commitFloating)
        if let pixels = floatingPixels8x8, let origin = floatingOrigin8x8 {
            for dy in 0..<pixels.count {
                for dx in 0..<pixels[dy].count {
                    let px = origin.x + dx
                    let py = origin.y + dy
                    guard px >= 0, px < 32, py >= 0, py < 32 else { continue }
                    spellCastTiles.tiles[castIndex].setPixel(x: px, y: py, value: pixels[dy][dx])
                }
            }
        }
        floatingPixels8x8 = nil
        floatingOrigin8x8 = nil
        moveAnchor8x8 = nil
        selection8x8 = nil
    }

    func clearSpellCastSelection() {
        if floatingPixels8x8 != nil {
            commitSpellCastFloating()
        }
        selection8x8 = nil
        selectionAnchor8x8 = nil
    }

    /// Lift pixels from the selected indicator tile into 8x8 floating state.
    func liftIndicatorSelection() {
        guard let sel = selection8x8, let idx = selectedIndicatorIndex else { return }
        pushUndo(actionKind: .liftSelection)
        var pixels = [[UInt8]](repeating: [UInt8](repeating: 0, count: sel.w), count: sel.h)
        for dy in 0..<sel.h {
            for dx in 0..<sel.w {
                let px = sel.x + dx
                let py = sel.y + dy
                guard px >= 0, px < 8, py >= 0, py < 8 else { continue }
                pixels[dy][dx] = indicatorTiles.tiles[idx].pixels[py][px]
                indicatorTiles.tiles[idx].pixels[py][px] = 0
            }
        }
        floatingPixels8x8 = pixels
        floatingOrigin8x8 = (x: sel.x, y: sel.y)
    }

    /// Stamp 8x8 floating pixels back onto the selected indicator tile.
    func commitIndicatorFloating() {
        guard let idx = selectedIndicatorIndex else { return }
        pushUndo(actionKind: .commitFloating)
        if let pixels = floatingPixels8x8, let origin = floatingOrigin8x8 {
            for dy in 0..<pixels.count {
                for dx in 0..<pixels[dy].count {
                    let px = origin.x + dx
                    let py = origin.y + dy
                    guard px >= 0, px < 8, py >= 0, py < 8 else { continue }
                    indicatorTiles.tiles[idx].pixels[py][px] = pixels[dy][dx]
                }
            }
        }
        floatingPixels8x8 = nil
        floatingOrigin8x8 = nil
        moveAnchor8x8 = nil
        selection8x8 = nil
    }

    func clearIndicatorSelection() {
        if floatingPixels8x8 != nil {
            commitIndicatorFloating()
        }
        selection8x8 = nil
        selectionAnchor8x8 = nil
    }

    // MARK: - 16x16 selection operations

    /// Copy pixels inside `selection` into `floatingPixels`, fill original area with 0.
    func liftSelection() {
        guard let sel = selection else { return }
        pushUndo(actionKind: .liftSelection)
        var pixels = [[UInt8]](repeating: [UInt8](repeating: 0, count: sel.w), count: sel.h)
        for dy in 0..<sel.h {
            for dx in 0..<sel.w {
                let px = sel.x + dx
                let py = sel.y + dy
                pixels[dy][dx] = tiles[selectedTileIndex].pixel(x: px, y: py)
                tiles[selectedTileIndex].setPixel(x: px, y: py, value: 0)
            }
        }
        floatingPixels = pixels
        floatingOrigin = (x: sel.x, y: sel.y)
    }

    /// Stamp `floatingPixels` at `floatingOrigin` (clipped to 0..<16), clear floating state and selection.
    func commitFloating() {
        pushUndo(actionKind: .commitFloating)
        if let pixels = floatingPixels, let origin = floatingOrigin {
            for dy in 0..<pixels.count {
                for dx in 0..<pixels[dy].count {
                    let px = origin.x + dx
                    let py = origin.y + dy
                    guard px >= 0, px < 16, py >= 0, py < 16 else { continue }
                    tiles[selectedTileIndex].setPixel(x: px, y: py, value: pixels[dy][dx])
                }
            }
        }
        floatingPixels = nil
        floatingOrigin = nil
        moveAnchor = nil
        selection = nil
    }

    /// If floating, commit first. Then nil out selection state.
    func clearSelection() {
        if floatingPixels != nil {
            commitFloating()
        }
        selection = nil
        selectionAnchor = nil
    }

    // MARK: - Palette editing

    /// Edit a palette color for the selected tile.
    func setPaletteColor(colorIndex: Int, color: GBColor) {
        pushUndo(actionKind: .paletteColor)
        let palIdx = selectedTile.paletteIndex
        palettes[palIdx].colors[colorIndex] = color
    }

    /// Edit one RGB component of a palette color for the selected tile.
    func setPaletteComponent(colorIndex: Int, component: KeyPath<GBColor, UInt8>, value: UInt8) {
        pushUndo(actionKind: .paletteComponent)
        let palIdx = selectedTile.paletteIndex
        switch component {
        case \.r: palettes[palIdx].colors[colorIndex].r = value
        case \.g: palettes[palIdx].colors[colorIndex].g = value
        case \.b: palettes[palIdx].colors[colorIndex].b = value
        default: break
        }
    }

    /// Change the palette index on the selected tile (called from the palette picker).
    func setTilePaletteIndex(_ newIndex: Int) {
        pushUndo(actionKind: .paletteIndex)
        tiles[selectedTileIndex].paletteIndex = newIndex
    }

    // MARK: - File I/O

    /// Load from game C source files (tiles_data.c + palettes.c) in the given directory.
    func loadFromGameDirectory(_ directory: URL) throws {
        let (loadedPalettes, loadedTiles, loadedSpellIcons, loadedIndicators, loadedCastTiles) = try CImporter.loadFromGameDirectory(directory)
        palettes = loadedPalettes
        tiles = loadedTiles
        spellIcons = loadedSpellIcons
        indicatorTiles = loadedIndicators
        spellCastTiles = loadedCastTiles
        selectedTileIndex = 0
    }

    /// Load a .gbtiles file, replacing the current document state.
    func loadFromFile(_ url: URL) throws {
        let (loadedPalettes, loadedTiles, loadedSpellIcons, loadedIndicators, loadedCastTiles) = try GBTilesFile.load(from: url)
        palettes = loadedPalettes
        tiles = loadedTiles
        spellIcons = loadedSpellIcons
        indicatorTiles = loadedIndicators
        spellCastTiles = loadedCastTiles
        selectedTileIndex = 0
        currentFileURL = url
    }

    /// Save to the current file, or to a new URL.
    /// If outputDirectory is set, also generates C files there.
    func saveToFile(_ url: URL? = nil) throws {
        let saveURL = url ?? currentFileURL
        guard let saveURL else { return }

        try GBTilesFile.save(palettes: palettes, tiles: tiles, spellIcons: spellIcons, spellCastTiles: spellCastTiles, indicatorTiles: indicatorTiles, to: saveURL)
        currentFileURL = saveURL

        // Also export C files if output directory is set
        if let outDir = outputDirectory {
            try CExporter.exportToDirectory(document: self, directory: outDir)
        }
    }

    // MARK: - Default palettes (seed for a new document)

    static func defaultPalettes() -> [GBPalette] {
        [
            // 0: Fire (red/orange)
            GBPalette(name: "Fire", colors: [
                GBColor(r: 31, g: 31, b: 31),
                GBColor(r: 29, g: 21, b: 9),
                GBColor(r: 28, g: 6, b: 2),
                GBColor(r: 21, g: 9, b: 5),
            ]),
            // 1: Water (blue/cyan)
            GBPalette(name: "Water", colors: [
                GBColor(r: 31, g: 31, b: 31),
                GBColor(r: 10, g: 19, b: 22),
                GBColor(r: 7, g: 15, b: 22),
                GBColor(r: 2, g: 5, b: 7),
            ]),
            // 2: Earth (green)
            GBPalette(name: "Earth", colors: [
                GBColor(r: 31, g: 31, b: 31),
                GBColor(r: 14, g: 20, b: 10),
                GBColor(r: 4, g: 18, b: 4),
                GBColor(r: 4, g: 6, b: 4),
            ]),
            // 3: Bronze (copper/brown)
            GBPalette(name: "Bronze", colors: [
                GBColor(r: 31, g: 31, b: 31),
                GBColor(r: 26, g: 18, b: 10),
                GBColor(r: 18, g: 12, b: 6),
                GBColor(r: 6, g: 4, b: 2),
            ]),
            // 4: Silver (white/cyan)
            GBPalette(name: "Silver", colors: [
                GBColor(r: 31, g: 31, b: 31),
                GBColor(r: 20, g: 20, b: 20),
                GBColor(r: 16, g: 20, b: 24),
                GBColor(r: 4, g: 4, b: 6),
            ]),
            // 5: Gold (yellow)
            GBPalette(name: "Gold", colors: [
                GBColor(r: 31, g: 31, b: 31),
                GBColor(r: 31, g: 28, b: 8),
                GBColor(r: 26, g: 20, b: 4),
                GBColor(r: 6, g: 4, b: 2),
            ]),
            // 6: UI (greyscale)
            GBPalette(name: "UI", colors: [
                GBColor(r: 31, g: 31, b: 31),
                GBColor(r: 20, g: 20, b: 20),
                GBColor(r: 10, g: 10, b: 10),
                GBColor(r: 0, g: 0, b: 0),
            ]),
            // 7: Dark / Obsidian
            GBPalette(name: "Dark / Obsidian", colors: [
                GBColor(r: 30, g: 30, b: 30),
                GBColor(r: 15, g: 15, b: 21),
                GBColor(r: 17, g: 7, b: 10),
                GBColor(r: 1, g: 1, b: 6),
            ]),
        ]
    }

    // MARK: - Default tiles (seed for a new document)

    static func defaultTiles() -> [GBTile16x16] {
        let tileBytes: [(name: String, palette: Int, data: [UInt8])] = [
            ("Empty", 6, [UInt8](repeating: 0, count: 64)),
            ("Fire", 0, [
                0x00,0x01, 0x01,0x01, 0x00,0x00, 0x01,0x01,
                0x03,0x06, 0x0E,0x0D, 0x14,0x1B, 0x18,0x17,
                0x00,0x00, 0x80,0x80, 0xC0,0xC0, 0x60,0xE0,
                0x40,0xC0, 0x80,0x80, 0x88,0x88, 0x98,0x98,
                0x20,0x3F, 0x20,0x3F, 0x20,0x3F, 0x20,0x3F,
                0x10,0x1F, 0x0A,0x0C, 0x0E,0x0C, 0x07,0x03,
                0x64,0xFC, 0x04,0xFC, 0x24,0xDC, 0x04,0xFC,
                0x58,0x38, 0x70,0x20, 0x60,0x20, 0xC0,0xC0,
            ]),
            ("Water", 1, [
                0x01,0x01, 0x01,0x01, 0x02,0x03, 0x02,0x03,
                0x02,0x03, 0x04,0x07, 0x05,0x06, 0x0B,0x0C,
                0x00,0x00, 0x00,0x00, 0x80,0x80, 0x80,0x80,
                0x80,0x80, 0x40,0xC0, 0x40,0xC0, 0xA0,0x60,
                0x13,0x1C, 0x27,0x38, 0x27,0x30, 0x27,0x30,
                0x13,0x18, 0x10,0x1C, 0x0C,0x0F, 0x03,0x03,
                0x90,0x70, 0xC8,0x38, 0xE8,0x18, 0xE8,0x18,
                0xD0,0x30, 0x10,0xF0, 0x60,0xE0, 0x80,0x80,
            ]),
            ("Earth", 2, [
                0x00,0x00, 0x01,0x01, 0x07,0x06, 0x00,0x0F,
                0x07,0x18, 0x1F,0x10, 0x1F,0x11, 0x2D,0x33,
                0x00,0x3C, 0xE4,0xE4, 0x8C,0x14, 0xDC,0x24,
                0xBC,0x44, 0x3C,0xC4, 0x74,0x8C, 0xF0,0x08,
                0x2E,0x37, 0x27,0x3C, 0x27,0x3C, 0x18,0x1F,
                0x1F,0x1F, 0x30,0x30, 0x60,0x60, 0x40,0x40,
                0x38,0xC8, 0xF0,0x18, 0x90,0x70, 0x60,0xE0,
                0x80,0x80, 0x00,0x00, 0x00,0x00, 0x00,0x00,
            ]),
            ("Bronze", 3, [
                0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
                0x07,0x07, 0x07,0x07, 0x19,0x07, 0x18,0x07,
                0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
                0xF0,0xF0, 0xF0,0xF0, 0x08,0xF0, 0x88,0xF0,
                0x19,0x07, 0x18,0x07, 0x19,0x07, 0x18,0x07,
                0x1F,0x00, 0x1F,0x00, 0x00,0x00, 0x00,0x00,
                0x08,0xF0, 0x88,0xF0, 0x08,0xF0, 0x88,0xF0,
                0xF8,0x00, 0xF8,0x00, 0x00,0x00, 0x00,0x00,
            ]),
            ("Silver", 4, [
                0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
                0x07,0x07, 0x07,0x07, 0x19,0x07, 0x18,0x07,
                0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
                0xF0,0xF0, 0xF0,0xF0, 0x08,0xF0, 0x88,0xF0,
                0x19,0x07, 0x18,0x07, 0x19,0x07, 0x18,0x07,
                0x1F,0x00, 0x1F,0x00, 0x00,0x00, 0x00,0x00,
                0x08,0xF0, 0x88,0xF0, 0x08,0xF0, 0x88,0xF0,
                0xF8,0x00, 0xF8,0x00, 0x00,0x00, 0x00,0x00,
            ]),
            ("Gold", 5, [
                0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
                0x07,0x07, 0x07,0x07, 0x19,0x07, 0x18,0x07,
                0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
                0xF0,0xF0, 0xF0,0xF0, 0x08,0xF0, 0x88,0xF0,
                0x19,0x07, 0x18,0x07, 0x19,0x07, 0x18,0x07,
                0x1F,0x00, 0x1F,0x00, 0x00,0x00, 0x00,0x00,
                0x08,0xF0, 0x88,0xF0, 0x08,0xF0, 0x88,0xF0,
                0xF8,0x00, 0xF8,0x00, 0x00,0x00, 0x00,0x00,
            ]),
            ("Plat", 4, [
                0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
                0x07,0x07, 0x07,0x07, 0x1F,0x06, 0x1F,0x07,
                0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
                0xF0,0xF0, 0xF0,0x70, 0xF8,0xF0, 0xF8,0x70,
                0x1F,0x06, 0x1F,0x07, 0x1F,0x06, 0x1F,0x07,
                0x1F,0x00, 0x1F,0x00, 0x00,0x00, 0x00,0x00,
                0xF8,0xF0, 0xF8,0x70, 0xF8,0xF0, 0xF8,0xF0,
                0xF8,0x00, 0xF8,0x00, 0x00,0x00, 0x00,0x00,
            ]),
            ("Emerald", 2, [
                0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
                0x01,0x01, 0x03,0x01, 0x07,0x03, 0x0F,0x03,
                0x00,0x00, 0x00,0x00, 0x80,0x80, 0x80,0x80,
                0xC0,0xC0, 0xE0,0xE0, 0xC0,0xF0, 0xC0,0xF8,
                0x1F,0x01, 0x3F,0x03, 0x1F,0x05, 0x0F,0x09,
                0x07,0x01, 0x03,0x01, 0x01,0x01, 0x00,0x00,
                0xC8,0xFC, 0xD0,0xFE, 0xE0,0xFC, 0xC0,0xF8,
                0xC0,0xF0, 0xC0,0xE0, 0xC0,0xC0, 0x00,0x00,
            ]),
            ("Ruby", 0, [
                0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
                0x00,0x01, 0x02,0x01, 0x07,0x03, 0x0E,0x03,
                0x00,0x00, 0x00,0x00, 0x00,0x80, 0x00,0x80,
                0x00,0xC0, 0x00,0xE0, 0x00,0xF0, 0x00,0xF8,
                0x1E,0x01, 0x3C,0x03, 0x1A,0x05, 0x06,0x09,
                0x06,0x01, 0x02,0x01, 0x00,0x01, 0x00,0x00,
                0x00,0xFC, 0x00,0xFE, 0x00,0xFC, 0x00,0xF8,
                0x00,0xF0, 0x00,0xE0, 0x00,0xC0, 0x00,0x00,
            ]),
            ("Obsid", 7, [
                0x00,0x00, 0x03,0x03, 0x0F,0x0E, 0x1D,0x1E,
                0x33,0x3D, 0x2C,0x34, 0x7B,0x6D, 0x66,0x5B,
                0x00,0x00, 0xF0,0xF0, 0xF8,0x38, 0x9C,0xEC,
                0x66,0xBE, 0xDE,0x6E, 0x37,0xDF, 0xCF,0x77,
                0x79,0x4E, 0x76,0x5B, 0x6D,0x76, 0x33,0x3D,
                0x3C,0x37, 0x1F,0x1F, 0x0F,0x0F, 0x00,0x00,
                0xB3,0xDF, 0x6F,0xB7, 0x9F,0xEF, 0x66,0xBE,
                0xDE,0x6E, 0x7C,0xFC, 0xF8,0xF8, 0x00,0x00,
            ]),
            ("Aether", 1, [
                0x00,0x00, 0x00,0x00, 0x20,0x20, 0x70,0x70,
                0x20,0x20, 0x03,0x03, 0x07,0x07, 0x0F,0x0F,
                0x00,0x00, 0x00,0x00, 0x00,0x00, 0x04,0x04,
                0x0E,0x0E, 0xE4,0xE4, 0xF0,0xF0, 0xF8,0xF8,
                0x0F,0x0F, 0x0F,0x0F, 0x0F,0x0F, 0x0F,0x0F,
                0x17,0x17, 0x3B,0x3B, 0x10,0x10, 0x00,0x00,
                0xF8,0xF8, 0xF8,0xF8, 0xF8,0xF8, 0xF8,0xF8,
                0xFC,0xFC, 0xE8,0xE8, 0x00,0x00, 0x00,0x00,
            ]),
        ]

        return tileBytes.map { entry in
            GBTile16x16(name: entry.name, bytes: entry.data, paletteIndex: entry.palette)
        }
    }
}
