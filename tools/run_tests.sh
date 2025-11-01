#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=${1:-../build}
TEST_MODE=${2:-""}

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "Build directory '$BUILD_DIR' does not exist. Run build.sh first." >&2
  exit 2
fi

pushd "$BUILD_DIR" >/dev/null

# If TEST_MODE not specified, prompt user
if [[ -z "$TEST_MODE" ]]; then
  echo ""
  echo "Select tests to run:"
  echo "  1) Unit tests (tests_catch2.exe)"
  echo "  2) Performance benchmarks (performance_benchmarks.exe)"
  echo "  3) Both"
  echo ""
  read -p "Enter choice (1-3): " choice
  
  case "$choice" in
    1) TEST_MODE="unit" ;;
    2) TEST_MODE="performance" ;;
    3) TEST_MODE="both" ;;
    *)
      echo "Invalid choice. Please enter 1, 2, or 3." >&2
      popd >/dev/null
      exit 1
      ;;
  esac
fi

exit_code=0

# Run unit tests
if [[ "$TEST_MODE" == "unit" || "$TEST_MODE" == "both" ]]; then
  UNIT_TEST_EXE="tests_catch2.exe"
  if [[ ! -x "$UNIT_TEST_EXE" && ! -f "$UNIT_TEST_EXE" ]]; then
    echo "Unit test executable '$UNIT_TEST_EXE' not found in $(pwd)" >&2
    popd >/dev/null
    exit 3
  fi
  
  echo ""
  echo "[run_tests.sh] Running unit tests: $(pwd)/$UNIT_TEST_EXE"
  ./"$UNIT_TEST_EXE" || exit_code=$?
fi

# Run performance benchmarks
if [[ "$TEST_MODE" == "performance" || "$TEST_MODE" == "both" ]]; then
  PERF_TEST_EXE="performance_benchmarks.exe"
  if [[ ! -x "$PERF_TEST_EXE" && ! -f "$PERF_TEST_EXE" ]]; then
    echo "Warning: Performance benchmark executable '$PERF_TEST_EXE' not found in $(pwd)" >&2
    if [[ "$TEST_MODE" == "performance" ]]; then
      popd >/dev/null
      exit 3
    fi
  else
    echo ""
    echo "[run_tests.sh] Running performance benchmarks: $(pwd)/$PERF_TEST_EXE"
    ./"$PERF_TEST_EXE" || exit_code=$?
  fi
fi

popd >/dev/null
exit $exit_code
