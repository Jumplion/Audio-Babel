#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=${1:-../build}
TEST_EXE=${2:-tests_runner.exe}

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "Build directory '$BUILD_DIR' does not exist. Run build.sh first." >&2
  exit 2
fi

pushd "$BUILD_DIR" >/dev/null

if [[ ! -x "$TEST_EXE" && ! -f "$TEST_EXE" ]]; then
  echo "Test executable '$TEST_EXE' not found in $(pwd)" >&2
  popd >/dev/null
  exit 3
fi

echo "[run_tests.sh] Running tests: $(pwd)/$TEST_EXE"
./"$TEST_EXE"

popd >/dev/null
