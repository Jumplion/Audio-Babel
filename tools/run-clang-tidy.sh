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
# Limit clang-tidy header analysis to our repository source folders so external
# headers (for example Boost's cpp_int.hpp) are not linted. The regex below
# matches paths containing cpp/, include/ or tests/.
echo "Running clang-tidy on ${#FILES[@]} files. Output -> $OUTPUT"

# Run clang-tidy but post-filter its textual output so only diagnostics that
# reference the files provided on the command line are shown. This avoids the
# brittle JSON --line-filter quoting and prevents included external headers
# (for example Boost) from appearing in the output.
HEADER_FILTER='^$'

# Build patterns for absolute paths and basenames
ABS_PATTERNS=()
BASENAME_PATTERNS=()
for f in "${FILES[@]}"; do
  # prefer realpath if available, fallback to the given path
  if command -v realpath >/dev/null 2>&1; then
    abs="$ (realpath "$f")"
  else
    abs="$f"
  fi
  # escape regex meta-characters
  esc_abs=$(printf "%s" "$abs" | sed -e 's/[].\\^$*+?()[{}|]/\\&/g')
  esc_base=$(printf "%s" "$(basename "$f")" | sed -e 's/[].\\^$*+?()[{}|]/\\&/g')
  ABS_PATTERNS+=("$esc_abs")
  BASENAME_PATTERNS+=("$esc_base")
done
ALL_PATTERN=$(printf "%s|" "${ABS_PATTERNS[@]}" "${BASENAME_PATTERNS[@]}" | sed 's/|$//')

echo "Filtering clang-tidy output for: $ALL_PATTERN"

# Run clang-tidy and filter output
RAW_OUTPUT=$(clang-tidy -p "$BUILD_DIR" -header-filter="$HEADER_FILTER" "${FILES[@]}" 2>&1 || true)
echo "$RAW_OUTPUT" | grep -E "$ALL_PATTERN" | tee "$OUTPUT"
ec=${PIPESTATUS[0]:-0}
exit $ec
