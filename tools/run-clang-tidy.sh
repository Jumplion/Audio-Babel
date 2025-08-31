#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/../build"
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
  echo "compile_commands.json not found in $BUILD_DIR. Run: cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON" >&2
  exit 2
fi

if [ "$#" -eq 0 ]; then
  mapfile -t FILES < <(find "$SCRIPT_DIR/../cpp/src" -name '*.cpp')
else
  FILES=("$@")
fi

if [ "${#FILES[@]}" -eq 0 ]; then
  echo "No .cpp files found" >&2
  exit 3
fi

OUTPUT="$SCRIPT_DIR/clang-tidy-output.txt"
echo "Running clang-tidy on ${#FILES[@]} files. Output -> $OUTPUT"
clang-tidy -p "$BUILD_DIR" "${FILES[@]}" 2>&1 | tee "$OUTPUT"
# preserve clang-tidy exit code from pipeline
exit ${PIPESTATUS[0]}
