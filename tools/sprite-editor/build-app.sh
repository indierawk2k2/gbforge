#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"

echo "Building GBSpriteEditor (release)..."
swift build -c release

echo "Creating app bundle..."
rm -rf GBSpriteEditor.app
mkdir -p GBSpriteEditor.app/Contents/MacOS

cat > GBSpriteEditor.app/Contents/Info.plist << 'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key>
    <string>com.gbspriteeditor.app</string>
    <key>CFBundleExecutable</key>
    <string>GBSpriteEditor</string>
    <key>CFBundleName</key>
    <string>GBSpriteEditor</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>LSMinimumSystemVersion</key>
    <string>14.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>NSSupportsAutomaticTermination</key>
    <false/>
</dict>
</plist>
PLIST

cp .build/release/GBSpriteEditor GBSpriteEditor.app/Contents/MacOS/

echo "Code signing..."
codesign --force --sign - GBSpriteEditor.app

echo "Done! GBSpriteEditor.app is ready."
