import SwiftUI
import AppKit

/// 16x16 pixel editing canvas with mouse-button-based color selection.
/// Supports Shift+drag to select a rectangle, then click-drag to move pixels.
/// Left click = color 0, Right click = color 1,
/// Cmd+Left = color 2, Cmd+Right = color 3.
struct TileGridView: View {
    @ObservedObject var document: TileDocument

    private let cellSize: CGFloat = 28
    private let gridSize = 16

    var body: some View {
        let totalSize = cellSize * CGFloat(gridSize)
        let tile = document.selectedTile

        ZStack {
            Canvas { context, size in
                // Draw pixel cells
                let palette = document.selectedPalette
                for row in 0..<gridSize {
                    for col in 0..<gridSize {
                        let colorIndex = Int(tile.pixel(x: col, y: row))
                        let color = palette[colorIndex].color
                        let rect = CGRect(
                            x: CGFloat(col) * cellSize,
                            y: CGFloat(row) * cellSize,
                            width: cellSize,
                            height: cellSize
                        )
                        context.fill(Path(rect), with: .color(color))
                    }
                }

                // Draw floating pixels at their current origin
                if let pixels = document.floatingPixels, let origin = document.floatingOrigin {
                    for dy in 0..<pixels.count {
                        for dx in 0..<pixels[dy].count {
                            let px = origin.x + dx
                            let py = origin.y + dy
                            guard px >= 0, px < gridSize, py >= 0, py < gridSize else { continue }
                            let color = palette[Int(pixels[dy][dx])].color
                            let rect = CGRect(
                                x: CGFloat(px) * cellSize,
                                y: CGFloat(py) * cellSize,
                                width: cellSize,
                                height: cellSize
                            )
                            context.fill(Path(rect), with: .color(color))
                        }
                    }
                }

                // Draw grid lines
                let thin = Path { path in
                    for i in 1..<gridSize {
                        let pos = CGFloat(i) * cellSize
                        path.move(to: CGPoint(x: pos, y: 0))
                        path.addLine(to: CGPoint(x: pos, y: totalSize))
                        path.move(to: CGPoint(x: 0, y: pos))
                        path.addLine(to: CGPoint(x: totalSize, y: pos))
                    }
                }
                context.stroke(thin, with: .color(.gray.opacity(0.3)), lineWidth: 0.5)

                // Draw 8x8 quadrant dividers (thicker)
                let thick = Path { path in
                    let mid = cellSize * 8
                    path.move(to: CGPoint(x: mid, y: 0))
                    path.addLine(to: CGPoint(x: mid, y: totalSize))
                    path.move(to: CGPoint(x: 0, y: mid))
                    path.addLine(to: CGPoint(x: totalSize, y: mid))
                }
                context.stroke(thick, with: .color(.gray.opacity(0.6)), lineWidth: 2)

                // Draw border
                let border = Path(CGRect(x: 0, y: 0, width: totalSize, height: totalSize))
                context.stroke(border, with: .color(.gray), lineWidth: 1)

                // Draw selection rectangle
                if let sel = document.selection {
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
                if let pixels = document.floatingPixels, let origin = document.floatingOrigin {
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
            .frame(width: totalSize, height: totalSize)

            // NSView overlay for mouse button detection
            MouseTrackingOverlay(
                cellSize: cellSize,
                gridSize: gridSize,
                onMouseDown: { col, row, colorIndex, hasShift in
                    guard col >= 0, col < gridSize, row >= 0, row < gridSize else { return }

                    if hasShift {
                        // Shift+click: start selection
                        if document.floatingPixels != nil {
                            document.commitFloating()
                        }
                        document.selectionAnchor = (x: col, y: row)
                        document.selection = (x: col, y: row, w: 1, h: 1)
                        return
                    }

                    // Non-shift click
                    if let sel = document.selection, document.floatingPixels == nil {
                        // Check if click is inside selection
                        if col >= sel.x && col < sel.x + sel.w &&
                           row >= sel.y && row < sel.y + sel.h {
                            // Lift and begin move
                            document.liftSelection()
                            document.moveAnchor = (x: col, y: row)
                            return
                        }
                    }

                    // Click outside selection or no selection: clear and paint
                    document.clearSelection()
                    document.handleGridClick(x: col, y: row, colorIndex: colorIndex, isDown: true)
                },
                onMouseDrag: { col, row, colorIndex, hasShift in
                    if hasShift, let anchor = document.selectionAnchor {
                        // Update selection rectangle from anchor to current position
                        let clampedCol = max(0, min(gridSize - 1, col))
                        let clampedRow = max(0, min(gridSize - 1, row))
                        let minX = min(anchor.x, clampedCol)
                        let maxX = max(anchor.x, clampedCol)
                        let minY = min(anchor.y, clampedRow)
                        let maxY = max(anchor.y, clampedRow)
                        document.selection = (x: minX, y: minY, w: maxX - minX + 1, h: maxY - minY + 1)
                        return
                    }

                    if document.floatingPixels != nil, let anchor = document.moveAnchor, let origin = document.floatingOrigin {
                        // Move floating pixels
                        let dx = col - anchor.x
                        let dy = row - anchor.y
                        document.floatingOrigin = (x: origin.x + dx, y: origin.y + dy)
                        document.moveAnchor = (x: col, y: row)
                        return
                    }

                    // Normal paint drag
                    guard col >= 0, col < gridSize, row >= 0, row < gridSize else { return }
                    if document.selection == nil {
                        document.handleGridClick(x: col, y: row, colorIndex: colorIndex, isDown: false)
                    }
                },
                onMouseUp: { col, row in
                    if document.selectionAnchor != nil {
                        // Finalize selection, clear anchor
                        document.selectionAnchor = nil
                        return
                    }

                    if document.floatingPixels != nil {
                        // Commit floating pixels on mouse up
                        document.commitFloating()
                        return
                    }
                }
            )
            .frame(width: totalSize, height: totalSize)
        }
    }
}

/// NSViewRepresentable that captures left/right mouse buttons and modifier keys.
struct MouseTrackingOverlay: NSViewRepresentable {
    let cellSize: CGFloat
    let gridSize: Int
    let onMouseDown: (Int, Int, Int, Bool) -> Void   // (col, row, colorIndex, hasShift)
    let onMouseDrag: (Int, Int, Int, Bool) -> Void   // (col, row, colorIndex, hasShift)
    let onMouseUp: (Int, Int) -> Void                // (col, row)

    func makeNSView(context: Context) -> TrackingNSView {
        let view = TrackingNSView()
        view.cellSize = cellSize
        view.gridSize = gridSize
        view.onMouseDown = onMouseDown
        view.onMouseDrag = onMouseDrag
        view.onMouseUp = onMouseUp
        return view
    }

    func updateNSView(_ nsView: TrackingNSView, context: Context) {
        nsView.cellSize = cellSize
        nsView.gridSize = gridSize
        nsView.onMouseDown = onMouseDown
        nsView.onMouseDrag = onMouseDrag
        nsView.onMouseUp = onMouseUp
    }

    class TrackingNSView: NSView {
        var cellSize: CGFloat = 28
        var gridSize: Int = 16
        var onMouseDown: ((Int, Int, Int, Bool) -> Void)?
        var onMouseDrag: ((Int, Int, Int, Bool) -> Void)?
        var onMouseUp: ((Int, Int) -> Void)?

        override var acceptsFirstResponder: Bool { true }

        override func mouseDown(with event: NSEvent) {
            let (col, row) = cellPosition(for: event)
            let colorIndex = colorIndex(for: event, isRight: false)
            let hasShift = event.modifierFlags.contains(.shift)
            onMouseDown?(col, row, colorIndex, hasShift)
        }

        override func mouseDragged(with event: NSEvent) {
            let (col, row) = cellPosition(for: event)
            let colorIndex = colorIndex(for: event, isRight: false)
            let hasShift = event.modifierFlags.contains(.shift)
            onMouseDrag?(col, row, colorIndex, hasShift)
        }

        override func mouseUp(with event: NSEvent) {
            let (col, row) = cellPosition(for: event)
            onMouseUp?(col, row)
        }

        override func rightMouseDown(with event: NSEvent) {
            let (col, row) = cellPosition(for: event)
            let colorIndex = colorIndex(for: event, isRight: true)
            let hasShift = event.modifierFlags.contains(.shift)
            onMouseDown?(col, row, colorIndex, hasShift)
        }

        override func rightMouseDragged(with event: NSEvent) {
            let (col, row) = cellPosition(for: event)
            let colorIndex = colorIndex(for: event, isRight: true)
            let hasShift = event.modifierFlags.contains(.shift)
            onMouseDrag?(col, row, colorIndex, hasShift)
        }

        override func rightMouseUp(with event: NSEvent) {
            let (col, row) = cellPosition(for: event)
            onMouseUp?(col, row)
        }

        // Suppress context menu on right-click
        override func menu(for event: NSEvent) -> NSMenu? { nil }

        private func cellPosition(for event: NSEvent) -> (Int, Int) {
            let loc = convert(event.locationInWindow, from: nil)
            let flipped = CGPoint(x: loc.x, y: bounds.height - loc.y)
            return (Int(flipped.x / cellSize), Int(flipped.y / cellSize))
        }

        private func colorIndex(for event: NSEvent, isRight: Bool) -> Int {
            let hasCmd = event.modifierFlags.contains(.command)
            if !isRight && !hasCmd { return 0 }
            else if isRight && !hasCmd { return 1 }
            else if !isRight && hasCmd { return 2 }
            else { return 3 }
        }
    }
}
