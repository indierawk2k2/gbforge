import Foundation

/// Palette index constants parsed from the game's `palettes.h`.
///
/// `tile_palette_map` in tiles_data.c names its palettes (PAL_SILVER, PAL_DARK,
/// ...) instead of writing literal indices, so the header is the only thing that
/// says which CRAM slot each tile actually uses. Importing the map means
/// resolving those names; exporting it means writing them back, so re-indexing a
/// palette in the header keeps working without anyone editing the table.
struct PaletteConstants {

    /// Every `#define PAL_* <int>` in the header, by name.
    private(set) var values: [String: Int] = [:]

    /// Background palette names by index — the reverse lookup used on export.
    /// Only the background block counts: the sprite palettes reuse the same
    /// indices under different names (PAL_CURSOR is also 0).
    private(set) var backgroundNames: [Int: String] = [:]

    static let empty = PaletteConstants()

    init() {}

    /// Parse `palettes.h` from a game resource directory.
    /// Returns nil if the header is missing or unreadable.
    static func load(fromGameDirectory directory: URL) -> PaletteConstants? {
        let url = directory.appendingPathComponent("palettes.h")
        guard let source = try? String(contentsOf: url, encoding: .utf8) else { return nil }
        return PaletteConstants(header: source)
    }

    init(header source: String) {
        // The header groups its indices under "// Background palette indices"
        // and "// Sprite palette indices" headings; only the former applies to
        // background tiles.
        var inBackgroundBlock = false

        for rawLine in source.split(separator: "\n", omittingEmptySubsequences: false) {
            let line = rawLine.trimmingCharacters(in: .whitespaces)

            if line.hasPrefix("//") {
                let lowered = line.lowercased()
                if lowered.contains("background palette") {
                    inBackgroundBlock = true
                } else if lowered.contains("sprite palette") {
                    inBackgroundBlock = false
                }
                continue
            }

            guard let (name, value) = Self.parseDefine(line) else { continue }
            values[name] = value
            // First name wins — the background block assigns each index once.
            if inBackgroundBlock && backgroundNames[value] == nil {
                backgroundNames[value] = name
            }
        }
    }

    /// Parse `#define NAME <int>`, ignoring any trailing comment.
    private static func parseDefine(_ line: String) -> (name: String, value: Int)? {
        let directive = "#define"
        guard line.hasPrefix(directive) else { return nil }
        let fields = line.dropFirst(directive.count)
            .split(whereSeparator: { $0 == " " || $0 == "\t" })
        guard fields.count >= 2, let value = Int(fields[1]) else { return nil }
        return (String(fields[0]), value)
    }

    /// Resolve a palette map entry that is either a literal index or a constant name.
    func resolve(_ token: String) -> Int? {
        if let literal = Int(token) { return literal }
        return values[token]
    }

    /// The constant name for a background palette index, if the header defines one.
    func backgroundName(for index: Int) -> String? {
        backgroundNames[index]
    }
}
