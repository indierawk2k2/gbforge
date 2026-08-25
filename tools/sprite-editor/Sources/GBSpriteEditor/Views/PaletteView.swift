import SwiftUI
import AppKit

/// Palette display with color swatches, HSV color picker square, and RGB555 editing.
struct PaletteView: View {
    @ObservedObject var document: TileDocument

    /// The active palette index: indicator's fixed palette if one is selected, otherwise the 16x16 tile's palette.
    private var activePaletteIndex: Int {
        if let idx = document.selectedIndicatorIndex,
           let kind = IndicatorKind(rawValue: idx) {
            return kind.paletteIndex
        }
        return document.selectedTile.paletteIndex
    }

    var body: some View {
        let palIdx = activePaletteIndex
        let palette = document.palettes[palIdx]
        let ci = document.selectedColorIndex
        let c = palette[ci]

        ScrollView {
            VStack(alignment: .leading, spacing: 10) {
                // Palette selector dropdown (disabled when indicator is selected — fixed palette)
                HStack {
                    Text("Palette")
                        .font(.headline)
                    if document.selectedIndicatorIndex != nil {
                        Text("\(palIdx): \(palette.name)")
                            .foregroundStyle(.secondary)
                    } else {
                        Picker("", selection: tilePaletteBinding) {
                            ForEach(0..<document.palettes.count, id: \.self) { i in
                                Text("\(i): \(document.palettes[i].name)")
                                    .tag(i)
                            }
                        }
                        .labelsHidden()
                    }
                }

                Divider()

                // Color swatches
                HStack(spacing: 6) {
                    ForEach(0..<4, id: \.self) { i in
                        colorSwatch(index: i, palette: palette)
                    }
                }

                // Mouse button legend
                HStack(spacing: 0) {
                    Text("L")
                        .font(.caption2.monospaced())
                        .frame(width: 36)
                    Text("R")
                        .font(.caption2.monospaced())
                        .frame(width: 36)
                    Text("\u{2318}L")
                        .font(.caption2.monospaced())
                        .frame(width: 36)
                    Text("\u{2318}R")
                        .font(.caption2.monospaced())
                        .frame(width: 36)
                }
                .foregroundStyle(.secondary)

                Divider()

                // Color picker square: X = Hue, Y = Value
                Text("Color \(ci)")
                    .font(.subheadline)
                    .foregroundStyle(.secondary)

                let hsv = rgbToHSV(c)
                ColorPickerSquare(
                    hue: hsv.h,
                    saturation: hsv.s,
                    value: hsv.v,
                    onPick: { h, v in
                        let newColor = hsvToGBColor(h: h, s: hsv.s, v: v)
                        document.setPaletteColor(colorIndex: ci, color: newColor)
                    }
                )

                // Current color preview
                HStack(spacing: 8) {
                    RoundedRectangle(cornerRadius: 4)
                        .fill(c.color)
                        .frame(width: 32, height: 32)
                        .overlay(
                            RoundedRectangle(cornerRadius: 4)
                                .stroke(Color.gray.opacity(0.3), lineWidth: 1)
                        )
                    VStack(alignment: .leading, spacing: 2) {
                        Text("RGB555: (\(c.r), \(c.g), \(c.b))")
                            .font(.caption.monospaced())
                        Text("0x\(String(format: "%04X", c.raw))")
                            .font(.caption.monospaced())
                            .foregroundStyle(.secondary)
                    }
                }

                Divider()

                // Saturation slider
                hsvSlider(label: "Sat", displayValue: Int(round(hsv.s * 100)),
                          unit: "%", sliderValue: hsv.s, range: 0...1) { newS in
                    let newColor = hsvToGBColor(h: hsv.h, s: newS, v: hsv.v)
                    document.setPaletteColor(colorIndex: ci, color: newColor)
                }

                // Brightness / Value slider
                hsvSlider(label: "Val", displayValue: Int(round(hsv.v * 100)),
                          unit: "%", sliderValue: hsv.v, range: 0...1) { newV in
                    let newColor = hsvToGBColor(h: hsv.h, s: hsv.s, v: newV)
                    document.setPaletteColor(colorIndex: ci, color: newColor)
                }

                Divider()

                // RGB555 sliders
                rgbSlider(label: "R", value: c.r, tint: .red) { newVal in
                    document.setPaletteComponent(colorIndex: ci, component: \.r, value: newVal)
                }
                rgbSlider(label: "G", value: c.g, tint: .green) { newVal in
                    document.setPaletteComponent(colorIndex: ci, component: \.g, value: newVal)
                }
                rgbSlider(label: "B", value: c.b, tint: .blue) { newVal in
                    document.setPaletteComponent(colorIndex: ci, component: \.b, value: newVal)
                }

                Divider()

                // Screen eyedropper
                Button {
                    pickColorFromScreen(colorIdx: ci)
                } label: {
                    Label("Pick from Screen", systemImage: "eyedropper")
                }
                .help("Sample a color from anywhere on screen")
            }
            .padding()
        }
        .frame(width: 260)
    }

    // MARK: - Subviews

    private func colorSwatch(index: Int, palette: GBPalette) -> some View {
        let isSelected = document.selectedColorIndex == index
        return VStack(spacing: 2) {
            RoundedRectangle(cornerRadius: 4)
                .fill(palette[index].color)
                .frame(width: 36, height: 36)
                .overlay(
                    RoundedRectangle(cornerRadius: 4)
                        .stroke(isSelected ? Color.accentColor : Color.gray.opacity(0.3),
                                lineWidth: isSelected ? 3 : 1)
                )
                .onTapGesture {
                    document.selectedColorIndex = index
                }
            Text("\(index)")
                .font(.caption2.monospaced())
                .foregroundStyle(.secondary)
        }
    }

    private func rgbSlider(label: String, value: UInt8, tint: Color, onChange: @escaping (UInt8) -> Void) -> some View {
        HStack(spacing: 6) {
            Text(label)
                .font(.caption.monospaced())
                .frame(width: 14)
            Slider(
                value: Binding(
                    get: { Double(value) },
                    set: { onChange(UInt8(min(max($0, 0), 31))) }
                ),
                in: 0...31,
                step: 1
            )
            .tint(tint)
            Text("\(value)")
                .font(.caption.monospaced())
                .frame(width: 22, alignment: .trailing)
        }
    }

    private func hsvSlider(label: String, displayValue: Int, unit: String,
                           sliderValue: Double, range: ClosedRange<Double>,
                           onChange: @escaping (Double) -> Void) -> some View {
        HStack(spacing: 6) {
            Text(label)
                .font(.caption.monospaced())
                .frame(width: 24)
            Slider(
                value: Binding(
                    get: { sliderValue },
                    set: { onChange($0) }
                ),
                in: range
            )
            Text("\(displayValue)\(unit)")
                .font(.caption.monospaced())
                .frame(width: 36, alignment: .trailing)
        }
    }

    // MARK: - Actions

    private func pickColorFromScreen(colorIdx: Int) {
        let sampler = NSColorSampler()
        sampler.show { selectedColor in
            guard let color = selectedColor?.usingColorSpace(.sRGB) else { return }
            let gbColor = GBColor(
                r: UInt8(min(max(color.redComponent * 31.0 + 0.5, 0), 31)),
                g: UInt8(min(max(color.greenComponent * 31.0 + 0.5, 0), 31)),
                b: UInt8(min(max(color.blueComponent * 31.0 + 0.5, 0), 31))
            )
            document.setPaletteColor(colorIndex: colorIdx, color: gbColor)
        }
    }

    private var tilePaletteBinding: Binding<Int> {
        Binding(
            get: { document.selectedTile.paletteIndex },
            set: { document.setTilePaletteIndex($0) }
        )
    }
}

// MARK: - HSV Color Picker Square

/// 2D color picker: X = Hue (0-360), Y = Value (1 at top, 0 at bottom).
/// Rendered with GBC RGB555-quantized colors at the current saturation.
struct ColorPickerSquare: View {
    let hue: Double        // 0-360
    let saturation: Double // 0-1
    let value: Double      // 0-1
    let onPick: (Double, Double) -> Void  // (hue, value)

    private let squareSize: CGFloat = 220

    var body: some View {
        ZStack {
            Canvas { context, size in
                let cols = Int(size.width)
                let rows = Int(size.height)
                let cellW = size.width / CGFloat(cols)
                let cellH = size.height / CGFloat(rows)

                for row in 0..<rows {
                    for col in 0..<cols {
                        let h = Double(col) / Double(cols) * 360.0
                        let v = 1.0 - Double(row) / Double(rows)
                        let gbColor = hsvToGBColor(h: h, s: saturation, v: v)
                        let rect = CGRect(
                            x: CGFloat(col) * cellW,
                            y: CGFloat(row) * cellH,
                            width: cellW + 0.5,
                            height: cellH + 0.5
                        )
                        context.fill(Path(rect), with: .color(gbColor.color))
                    }
                }

                // Draw crosshair at current position
                let cx = CGFloat(hue / 360.0) * size.width
                let cy = CGFloat(1.0 - value) * size.height
                let crossSize: CGFloat = 8

                let hPath = Path { p in
                    p.move(to: CGPoint(x: cx - crossSize, y: cy))
                    p.addLine(to: CGPoint(x: cx + crossSize, y: cy))
                }
                let vPath = Path { p in
                    p.move(to: CGPoint(x: cx, y: cy - crossSize))
                    p.addLine(to: CGPoint(x: cx, y: cy + crossSize))
                }
                // White outline
                context.stroke(hPath, with: .color(.white), lineWidth: 3)
                context.stroke(vPath, with: .color(.white), lineWidth: 3)
                // Black inner
                context.stroke(hPath, with: .color(.black), lineWidth: 1)
                context.stroke(vPath, with: .color(.black), lineWidth: 1)
            }
            .frame(width: squareSize, height: squareSize * 0.6)

            // Mouse tracking overlay
            ColorPickerOverlay(size: CGSize(width: squareSize, height: squareSize * 0.6), onPick: onPick)
                .frame(width: squareSize, height: squareSize * 0.6)
        }
        .cornerRadius(4)
        .overlay(
            RoundedRectangle(cornerRadius: 4)
                .stroke(Color.gray.opacity(0.3), lineWidth: 1)
        )
    }
}

/// NSViewRepresentable for tracking mouse clicks/drags on the color picker square.
struct ColorPickerOverlay: NSViewRepresentable {
    let size: CGSize
    let onPick: (Double, Double) -> Void

    func makeNSView(context: Context) -> PickerNSView {
        let view = PickerNSView()
        view.pickerSize = size
        view.onPick = onPick
        return view
    }

    func updateNSView(_ nsView: PickerNSView, context: Context) {
        nsView.pickerSize = size
        nsView.onPick = onPick
    }

    class PickerNSView: NSView {
        var pickerSize: CGSize = .zero
        var onPick: ((Double, Double) -> Void)?

        override var acceptsFirstResponder: Bool { true }

        override func mouseDown(with event: NSEvent) { handleMouse(event) }
        override func mouseDragged(with event: NSEvent) { handleMouse(event) }

        private func handleMouse(_ event: NSEvent) {
            let loc = convert(event.locationInWindow, from: nil)
            let flipped = CGPoint(x: loc.x, y: bounds.height - loc.y)
            let hue = max(0, min(360, Double(flipped.x / bounds.width) * 360.0))
            let value = max(0, min(1, 1.0 - Double(flipped.y / bounds.height)))
            onPick?(hue, value)
        }
    }
}

// MARK: - HSV ↔ GBColor conversion

/// Convert GBColor (RGB555) to HSV.
func rgbToHSV(_ c: GBColor) -> (h: Double, s: Double, v: Double) {
    let r = Double(c.r) / 31.0
    let g = Double(c.g) / 31.0
    let b = Double(c.b) / 31.0

    let maxC = max(r, g, b)
    let minC = min(r, g, b)
    let delta = maxC - minC

    // Value
    let v = maxC

    // Saturation
    let s = maxC == 0 ? 0.0 : delta / maxC

    // Hue
    var h: Double = 0
    if delta > 0 {
        if maxC == r {
            h = 60.0 * fmod((g - b) / delta, 6.0)
        } else if maxC == g {
            h = 60.0 * ((b - r) / delta + 2.0)
        } else {
            h = 60.0 * ((r - g) / delta + 4.0)
        }
        if h < 0 { h += 360.0 }
    }

    return (h, s, v)
}

/// Convert HSV to GBColor (quantized to RGB555).
func hsvToGBColor(h: Double, s: Double, v: Double) -> GBColor {
    let s = max(0, min(1, s))
    let v = max(0, min(1, v))
    let h = fmod(max(0, h), 360.0)

    let c = v * s
    let x = c * (1.0 - abs(fmod(h / 60.0, 2.0) - 1.0))
    let m = v - c

    let r1, g1, b1: Double
    switch h {
    case 0..<60:    r1 = c; g1 = x; b1 = 0
    case 60..<120:  r1 = x; g1 = c; b1 = 0
    case 120..<180: r1 = 0; g1 = c; b1 = x
    case 180..<240: r1 = 0; g1 = x; b1 = c
    case 240..<300: r1 = x; g1 = 0; b1 = c
    default:        r1 = c; g1 = 0; b1 = x
    }

    return GBColor(
        r: UInt8(min(max((r1 + m) * 31.0 + 0.5, 0), 31)),
        g: UInt8(min(max((g1 + m) * 31.0 + 0.5, 0), 31)),
        b: UInt8(min(max((b1 + m) * 31.0 + 0.5, 0), 31))
    )
}
