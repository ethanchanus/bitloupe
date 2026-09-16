#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="$ROOT_DIR/src"
BUILD_DIR="$SRC_DIR/build-arm64"
OUT_DIR="$SRC_DIR/build/dmg"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required command: $1" >&2
    exit 1
  fi
}

require_cmd cmake
require_cmd cpack
require_cmd strip
require_cmd otool
require_cmd rg
require_cmd macdeployqt
require_cmd codesign
require_cmd /usr/libexec/PlistBuddy

mkdir -p "$OUT_DIR"

if [[ -z "${JOBS:-}" ]]; then
  JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
fi

echo "Configuring arm64 build..."
cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
  -DCMAKE_OSX_ARCHITECTURES="arm64" \
  -DCMAKE_BUILD_TYPE=Release

echo "Building SpeedCrunch (arm64, parallel jobs: $JOBS)..."
cmake --build "$BUILD_DIR" --config Release --target SpeedCrunch --parallel "$JOBS"

APP_BIN="$BUILD_DIR/SpeedCrunch.app/Contents/MacOS/SpeedCrunch"
APP_BUNDLE="$BUILD_DIR/SpeedCrunch.app"
APP_FRAMEWORKS_DIR="$APP_BUNDLE/Contents/Frameworks"
APP_PLIST="$APP_BUNDLE/Contents/Info.plist"
if [[ ! -f "$APP_BIN" ]]; then
  echo "Build failed (missing app binary $APP_BIN)." >&2
  exit 1
fi

echo "Stripping app binary..."
strip -x "$APP_BIN"

echo "Bundling Qt frameworks with macdeployqt..."
macdeployqt "$APP_BUNDLE" -always-overwrite

if [[ ! -d "$APP_FRAMEWORKS_DIR" ]] || [[ ! -e "$APP_FRAMEWORKS_DIR/QtCore.framework" ]]; then
  echo "Qt deployment failed (missing $APP_FRAMEWORKS_DIR/QtCore.framework)." >&2
  exit 1
fi

if otool -L "$APP_BIN" | rg -q '/opt/homebrew|/usr/local|Cellar'; then
  echo "Qt deployment failed: binary still links to Homebrew Qt paths." >&2
  otool -L "$APP_BIN" | rg 'Qt|/opt/homebrew|/usr/local|Cellar' >&2 || true
  exit 1
fi

echo "Re-signing bundled app..."
codesign --force --deep --sign - --timestamp=none "$APP_BUNDLE"

echo "Verifying app signature..."
codesign --verify --deep --strict --verbose=2 "$APP_BUNDLE"

echo "Generating DMG with CPack (DragNDrop)..."
(
  cd "$BUILD_DIR"
  cpack -G DragNDrop
)

DMG_PATH="$BUILD_DIR/SpeedCrunch.dmg"
APP_VERSION="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$APP_PLIST" 2>/dev/null || true)"
if [[ -z "$APP_VERSION" ]]; then
  echo "Unable to determine app version from $APP_PLIST." >&2
  exit 1
fi
OUT_PATH="$OUT_DIR/SpeedCrunch-${APP_VERSION}-arm64.dmg"
if [[ ! -f "$DMG_PATH" ]]; then
  echo "DMG generation failed (missing $DMG_PATH)." >&2
  exit 1
fi

cp -f "$DMG_PATH" "$OUT_PATH"
echo "DMG generated:"
ls -lh "$OUT_PATH"
