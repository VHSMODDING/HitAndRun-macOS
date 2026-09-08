#!/bin/bash
# Creates a small, self-contained installer DMG. It does not upload anything.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
OUT="$ROOT/dist/installer-v0.5.0"
APP="$OUT/Hit & Run Installer.app"
DMG="$ROOT/dist/HitAndRun-Installer-v0.5.0-macOS-arm64.dmg"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
xcrun swiftc -O -target arm64-apple-macos14.0 \
    "$ROOT/port/macos/installer/Installer.swift" \
    -o "$APP/Contents/MacOS/HitAndRunInstaller" -framework AppKit
ditto "$ROOT/port/macos/installer/Info.plist" "$APP/Contents/Info.plist"
ditto "$ROOT/assets/icons/AppIcon.icns" "$APP/Contents/Resources/AppIcon.icns"
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"
hdiutil create -ov -srcfolder "$OUT" -volname "Hit & Run Installer 0.5.0" -format ULFO "$DMG"
echo "Prepared: $DMG"
