#!/bin/bash
# Local packaging only: never contacts GitHub or modifies Git history.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VERSION=0.5.0
OUT="$ROOT/dist/v$VERSION"
APP="$OUT/The Simpsons Hit & Run.app"
SOURCE="$ROOT/build/macos-arm64/HitAndRunPortHost.app"
mkdir -p "$OUT"
if [[ -e "$APP" ]]; then
    echo "Release bundle already exists: $APP. Move it aside before rebuilding." >&2
    exit 1
fi
cmake --build "$ROOT/build/macos-arm64" --target HitAndRunPortHost -j 6
ditto "$SOURCE" "$APP"
BIN="$APP/Contents/MacOS/HitAndRunPortHost"
mkdir -p "$APP/Contents/Frameworks"
PNG="$(otool -L "$BIN" | awk '/\/.*libpng.*dylib/ {print $1; exit}')"
[[ -f "$PNG" ]] || { echo "Cannot locate linked libpng: $PNG" >&2; exit 1; }
ditto "$PNG" "$APP/Contents/Frameworks/libpng16.16.dylib"
install_name_tool -id '@executable_path/../Frameworks/libpng16.16.dylib' "$APP/Contents/Frameworks/libpng16.16.dylib"
install_name_tool -change "$PNG" '@executable_path/../Frameworks/libpng16.16.dylib' "$BIN"
for MACHO in "$BIN" "$APP/Contents/Frameworks/libpng16.16.dylib"; do
    if otool -L "$MACHO" | tail -n +2 | awk '{print $1}' | grep -Ev '^(/System/Library/|/usr/lib/|@executable_path/)' ; then
        echo "Unexpected external dependency in $MACHO" >&2; exit 1
    fi
done
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"
plutil -lint "$APP/Contents/Info.plist"
hdiutil create -srcfolder "$OUT" -volname "Hit & Run $VERSION" -format ULFO "$ROOT/dist/HitAndRun-v$VERSION-macOS-arm64.dmg"
cd "$ROOT/dist"
shasum -a 256 "HitAndRun-v$VERSION-macOS-arm64.dmg" > "HitAndRun-v$VERSION-macOS-arm64.dmg.sha256"
mkdir -p "v$VERSION-upload"
hdiutil segment -o "v$VERSION-upload/HitAndRun-v$VERSION-macOS-arm64.dmg" -segmentSize 1900m "HitAndRun-v$VERSION-macOS-arm64.dmg"
cd "v$VERSION-upload"
shasum -a 256 ./*.dmg ./*.dmgpart > SHA256SUMS.txt
echo "Prepared: $APP"
