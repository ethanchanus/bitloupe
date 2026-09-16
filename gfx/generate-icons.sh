#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SVG_FILE="$ROOT_DIR/gfx/speedcrunch.svg"
RES_DIR="$ROOT_DIR/src/resources"
DOC_DIR="$ROOT_DIR/doc/src"

PNG_OUT="$RES_DIR/speedcrunch.png"
DOC_LOGO_OUT="$DOC_DIR/logo.png"
ICO_OUT="$RES_DIR/speedcrunch.ico"
ICNS_OUT="$RES_DIR/speedcrunch.icns"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required command: $1" >&2
    exit 1
  fi
}

require_cmd rsvg-convert
require_cmd sips
require_cmd magick
require_cmd png2icns

if [[ ! -f "$SVG_FILE" ]]; then
  echo "SVG not found: $SVG_FILE" >&2
  exit 1
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

MASTER_PNG="$TMP_DIR/speedcrunch-1024.png"

echo "Rendering SVG -> 1024x1024 PNG..."
rsvg-convert -w 1024 -h 1024 "$SVG_FILE" -o "$MASTER_PNG"

echo "Generating Linux PNG (256x256)..."
sips -z 256 256 "$MASTER_PNG" --out "$PNG_OUT" >/dev/null
cp "$PNG_OUT" "$DOC_LOGO_OUT"

echo "Generating Windows ICO (16..256)..."
magick "$MASTER_PNG" -background none \
  -define icon:auto-resize=16,24,32,48,64,128,256 \
  "$ICO_OUT"

echo "Generating macOS ICNS..."
for s in 16 32 48 128; do
  sips -z "$s" "$s" "$MASTER_PNG" --out "$TMP_DIR/icon-${s}.png" >/dev/null
done
png2icns "$ICNS_OUT" \
  "$TMP_DIR/icon-16.png" \
  "$TMP_DIR/icon-32.png" \
  "$TMP_DIR/icon-48.png" \
  "$TMP_DIR/icon-128.png" >/dev/null

echo "Done."
file "$PNG_OUT" "$DOC_LOGO_OUT" "$ICO_OUT" "$ICNS_OUT"
