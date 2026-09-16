#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOC_SRC_DIR="$ROOT_DIR/doc/src"
VENV_BIN="$ROOT_DIR/doc/.venv/bin"
CLEAN_SCRIPT="$ROOT_DIR/doc/clean-doc-builds.sh"

if [[ ! -d "$DOC_SRC_DIR" ]]; then
  echo "error: doc source directory not found: $DOC_SRC_DIR" >&2
  exit 1
fi

if [[ -x "$CLEAN_SCRIPT" ]]; then
  "$CLEAN_SCRIPT"
else
  echo "error: clean script not found or not executable: $CLEAN_SCRIPT" >&2
  exit 1
fi

if [[ ! -x "$VENV_BIN/python" ]]; then
  echo "error: python venv not found at $VENV_BIN" >&2
  echo "Create it first, e.g.: python3 -m venv $ROOT_DIR/doc/.venv" >&2
  exit 1
fi

if ! command -v qhelpgenerator >/dev/null 2>&1; then
  for candidate in \
    /opt/homebrew/opt/qttools/share/qt/libexec \
    /opt/homebrew/Cellar/qttools/6.10.2/share/qt/libexec \
    /usr/lib/qt6/bin \
    /usr/lib/qt/bin
  do
    if [[ -x "$candidate/qhelpgenerator" ]]; then
      export PATH="$VENV_BIN:$candidate:$PATH"
      break
    fi
  done
else
  export PATH="$VENV_BIN:$PATH"
fi

if ! command -v qhelpgenerator >/dev/null 2>&1; then
  echo "error: qhelpgenerator not found in PATH" >&2
  echo "Install Qt tools and/or add qhelpgenerator to PATH." >&2
  exit 1
fi

cd "$DOC_SRC_DIR"
make build-bundled
make update-prebuilt-manual

echo "manual rebuilt and copied to: $ROOT_DIR/doc/build_html_embedded"
