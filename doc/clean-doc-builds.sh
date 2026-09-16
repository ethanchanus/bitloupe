#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOC_SRC_DIR="$ROOT_DIR/doc/src"

if [[ ! -d "$DOC_SRC_DIR" ]]; then
  echo "error: doc source directory not found: $DOC_SRC_DIR" >&2
  exit 1
fi

shopt -s nullglob
build_dirs=("$DOC_SRC_DIR"/_build-*)
shopt -u nullglob

if [[ ${#build_dirs[@]} -eq 0 ]]; then
  echo "no generated doc build directories found under: $DOC_SRC_DIR"
  exit 0
fi

rm -rf "${build_dirs[@]}"
echo "removed generated doc build directories under: $DOC_SRC_DIR"
