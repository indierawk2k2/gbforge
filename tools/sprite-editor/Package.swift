// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "GBSpriteEditor",
    platforms: [.macOS(.v14)],
    targets: [
        .executableTarget(
            name: "GBSpriteEditor",
            path: "Sources/GBSpriteEditor"
        )
    ]
)
