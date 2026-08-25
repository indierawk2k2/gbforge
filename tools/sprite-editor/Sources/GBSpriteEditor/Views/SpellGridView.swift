import SwiftUI

/// Pixel editing canvas for spell icons (8x8) and cast tiles (32x32).
/// Supports Shift+drag to select, click-drag to move, and mouse-button painting.
/// Left click = color 0, Right click = color 1,
/// Cmd+Left = color 2, Cmd+Right = color 3.
struct SpellGridView: View {
    @ObservedObject var document: TileDocument

    /// Dynamic grid size: 32 for cast tiles, 8 for spell icons.
    private var gridSize: Int {
        document.isSpellCastSelected ? 32 : 8
    }

    /// Dynamic cell size: smaller cells for the 32x32 canvas.
    private var cellSize: CGFloat {
        document.isSpellCastSelected ? 12 : 40
    }

    /// The palette for the currently selected spell.
    /// Disabled icon variants (indices 7-13) use grayscale.
    private var spellPalette: GBPalette {
        if document.isSpellCastSelected {
            let kind = SpellKind(rawValue: document.selectedSpellIndex - 14)!
            return document.palettes[kind.paletteIndex]
        }
        if document.selectedSpellIndex >= 7 {
            return GBPalette.grayscale
        }
        let kind = SpellKind(rawValue: document.selectedSpellIndex % 7)!
        return document.palettes[kind.paletteIndex]
    }

    var body: some View {
        let gs = gridSize
        let cs = cellSize
        let totalSize = cs * CGFloat(gs)
        let palette = spellPalette
        let c0 = palette[0].color
        let c1 = palette[1].color
        let c2 = palette[2].color
        let c3 = palette[3].color
        let isCast = document.isSpellCastSelected

        ZStack {
            Canvas { context, size in
                let colors = [c0, c1, c2, c3]

                // Draw pixel cells
                for row in 0..<gs {
                    for col in 0..<gs {
                        let colorIndex: Int
                        if isCast {
                            colorIndex = Int(document.spellCastTiles.tiles[document.selectedSpellIndex - 14].pixel(x: col, y: row))
                        } else {
                            colorIndex = Int(document.spellIcons.icons[document.selectedSpellIndex].pixels[row][col])
                        }
                        let rect = CGRect(
                            x: CGFloat(col) * cs,
                            y: CGFloat(row) * cs,
                            width: cs,
                            height: cs
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
                            guard px >= 0, px < gs, py >= 0, py < gs else { continue }
                            let rect = CGRect(
                                x: CGFloat(px) * cs,
                                y: CGFloat(py) * cs,
                                width: cs,
                                height: cs
                            )
                            context.fill(Path(rect), with: .color(colors[Int(pixels[dy][dx])]))
                        }
                    }
                }

                // Draw grid lines
                let lines = Path { path in
                    for i in 1..<gs {
                        let pos = CGFloat(i) * cs
                        path.move(to: CGPoint(x: pos, y: 0))
                        path.addLine(to: CGPoint(x: pos, y: totalSize))
                        path.move(to: CGPoint(x: 0, y: pos))
                        path.addLine(to: CGPoint(x: totalSize, y: pos))
                    }
                }
                context.stroke(lines, with: .color(.gray.opacity(0.3)), lineWidth: 0.5)

                // Draw heavier sub-tile boundary lines for 32x32 cast tiles
                if isCast {
                    let subLines = Path { path in
                        for i in 1..<4 {
                            let pos = CGFloat(i * 8) * cs
                            path.move(to: CGPoint(x: pos, y: 0))
                            path.addLine(to: CGPoint(x: pos, y: totalSize))
                            path.move(to: CGPoint(x: 0, y: pos))
                            path.addLine(to: CGPoint(x: totalSize, y: pos))
                        }
                    }
                    context.stroke(subLines, with: .color(.gray.opacity(0.6)), lineWidth: 1.5)
                }

                // Draw border
                let border = Path(CGRect(x: 0, y: 0, width: totalSize, height: totalSize))
                context.stroke(border, with: .color(.gray), lineWidth: 1)

                // Draw selection rectangle
                if let sel = document.selection8x8 {
                    let selRect = CGRect(
                        x: CGFloat(sel.x) * cs,
                        y: CGFloat(sel.y) * cs,
                        width: CGFloat(sel.w) * cs,
                        height: CGFloat(sel.h) * cs
                    )
                    let dashStyle = StrokeStyle(lineWidth: 2, dash: [6, 3])
                    context.stroke(Path(selRect), with: .color(.accentColor), style: dashStyle)
                }

                // Draw floating pixels outline
                if let pixels = document.floatingPixels8x8, let origin = document.floatingOrigin8x8 {
                    let floatRect = CGRect(
                        x: CGFloat(origin.x) * cs,
                        y: CGFloat(origin.y) * cs,
                        width: CGFloat(pixels[0].count) * cs,
                        height: CGFloat(pixels.count) * cs
                    )
                    let dashStyle = StrokeStyle(lineWidth: 2, dash: [4, 2])
                    context.stroke(Path(floatRect), with: .color(.orange), style: dashStyle)
                }
            }
            .id(document.selectedSpellIndex)
            .frame(width: totalSize, height: totalSize)

            SpellMouseOverlay(
                cellSize: cs,
                gridSize: gs,
                onMouseDown: { col, row, colorIndex, hasShift in
                    guard col >= 0, col < gs, row >= 0, row < gs else { return }

                    if hasShift {
                        if document.floatingPixels8x8 != nil {
                            if isCast { document.commitSpellCastFloating() }
                            else { document.commitSpellFloating() }
                        }
                        document.selectionAnchor8x8 = (x: col, y: row)
                        document.selection8x8 = (x: col, y: row, w: 1, h: 1)
                        return
                    }

                    if let sel = document.selection8x8, document.floatingPixels8x8 == nil {
                        if col >= sel.x && col < sel.x + sel.w &&
                           row >= sel.y && row < sel.y + sel.h {
                            if isCast { document.liftSpellCastSelection() }
                            else { document.liftSpellSelection() }
                            document.moveAnchor8x8 = (x: col, y: row)
                            return
                        }
                    }

                    if isCast { document.clearSpellCastSelection() }
                    else { document.clearSpellSelection() }

                    if isCast {
                        document.handleSpellCastGridClick(x: col, y: row, colorIndex: colorIndex, isDown: true)
                    } else {
                        document.handleSpellGridClick(x: col, y: row, colorIndex: colorIndex, isDown: true)
                    }
                },
                onMouseDrag: { col, row, colorIndex, hasShift in
                    if hasShift, let anchor = document.selectionAnchor8x8 {
                        let clampedCol = max(0, min(gs - 1, col))
                        let clampedRow = max(0, min(gs - 1, row))
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

                    guard col >= 0, col < gs, row >= 0, row < gs else { return }
                    if document.selection8x8 == nil {
                        if isCast {
                            document.handleSpellCastGridClick(x: col, y: row, colorIndex: colorIndex, isDown: false)
                        } else {
                            document.handleSpellGridClick(x: col, y: row, colorIndex: colorIndex, isDown: false)
                        }
                    }
                },
                onMouseUp: { _, _ in
                    if document.selectionAnchor8x8 != nil {
                        document.selectionAnchor8x8 = nil
                        return
                    }

                    if document.floatingPixels8x8 != nil {
                        if isCast { document.commitSpellCastFloating() }
                        else { document.commitSpellFloating() }
                        return
                    }
                }
            )
            .frame(width: totalSize, height: totalSize)
        }
    }
}
