#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=${1:-../build}
REMOVE_DIR=${2:-}

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "No build directory found at '$BUILD_DIR'. Nothing to clean."
  exit 0
fi

if [[ "$REMOVE_DIR" == "--remove" ]]; then
  echo "[clean.sh] Removing directory: $BUILD_DIR"
  rm -rf "$BUILD_DIR"
  exit $?
fi

echo "[clean.sh] Removing common build artifacts inside $BUILD_DIR"
shopt -s dotglob
for f in "$BUILD_DIR"/*; do
  case $(basename "$f") in
    .git|.gitignore) continue ;;
    *) rm -rf "$f" ;;
  esac
done

echo "Clean complete."
