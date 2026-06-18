#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=${1:-../build}
EXE=${2:-example_main}

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "Build directory '$BUILD_DIR' does not exist. Run build.sh first." >&2
  exit 2
fi

pushd "$BUILD_DIR" >/dev/null

if [[ ! -x "$EXE" && ! -f "$EXE" ]]; then
  echo "Example executable '$EXE' not found in $(pwd)" >&2
  popd >/dev/null
  exit 3
fi

echo "[run_example.sh] Running example: $(pwd)/$EXE"
./"$EXE"

popd >/dev/null
