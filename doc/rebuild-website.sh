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

export PATH="$VENV_BIN:$PATH"

cd "$DOC_SRC_DIR"
make build-html

echo "website rebuilt in: $ROOT_DIR/doc/src/_build-html"
