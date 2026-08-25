import SwiftUI
import AppKit

/// NSViewRepresentable that captures mouse events for 8x8 tile painting.
/// Resolves paint color from mouse button + Cmd modifier:
/// Left = 0, Right = 1, Cmd+Left = 2, Cmd+Right = 3.
/// Passes Shift state for selection support.
struct SpellMouseOverlay: NSViewRepresentable {
    let cellSize: CGFloat
    let gridSize: Int
    let onMouseDown: (Int, Int, Int, Bool) -> Void   // (col, row, colorIndex, hasShift)
    let onMouseDrag: (Int, Int, Int, Bool) -> Void   // (col, row, colorIndex, hasShift)
    let onMouseUp: (Int, Int) -> Void                // (col, row)

    func makeNSView(context: Context) -> SpellTrackingNSView {
        let view = SpellTrackingNSView()
        view.cellSize = cellSize
        view.gridSize = gridSize
        view.onMouseDown = onMouseDown
        view.onMouseDrag = onMouseDrag
        view.onMouseUp = onMouseUp
        return view
    }

    func updateNSView(_ nsView: SpellTrackingNSView, context: Context) {
        nsView.cellSize = cellSize
        nsView.gridSize = gridSize
        nsView.onMouseDown = onMouseDown
        nsView.onMouseDrag = onMouseDrag
        nsView.onMouseUp = onMouseUp
    }

    class SpellTrackingNSView: NSView {
        var cellSize: CGFloat = 40
        var gridSize: Int = 8
        var onMouseDown: ((Int, Int, Int, Bool) -> Void)?
        var onMouseDrag: ((Int, Int, Int, Bool) -> Void)?
        var onMouseUp: ((Int, Int) -> Void)?

        override var acceptsFirstResponder: Bool { true }

        override func mouseDown(with event: NSEvent) {
            let (col, row) = cellPosition(for: event)
            onMouseDown?(col, row, colorIndex(for: event, isRight: false), event.modifierFlags.contains(.shift))
        }

        override func mouseDragged(with event: NSEvent) {
            let (col, row) = cellPosition(for: event)
            onMouseDrag?(col, row, colorIndex(for: event, isRight: false), event.modifierFlags.contains(.shift))
        }

        override func mouseUp(with event: NSEvent) {
            let (col, row) = cellPosition(for: event)
            onMouseUp?(col, row)
        }

        override func rightMouseDown(with event: NSEvent) {
            let (col, row) = cellPosition(for: event)
            onMouseDown?(col, row, colorIndex(for: event, isRight: true), event.modifierFlags.contains(.shift))
        }

        override func rightMouseDragged(with event: NSEvent) {
            let (col, row) = cellPosition(for: event)
            onMouseDrag?(col, row, colorIndex(for: event, isRight: true), event.modifierFlags.contains(.shift))
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
