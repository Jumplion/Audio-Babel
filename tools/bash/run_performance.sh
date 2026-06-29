#!/bin/bash
#
# Run performance benchmarks for the Audio Babel library.
# Builds in Release mode and executes the performance benchmark suite.
# Results are written to build/performance_results.txt.
#
# Usage:
#   ./run_performance.sh
#   ./run_performance.sh clean
#   ./run_performance.sh compare
#   ./run_performance.sh clean compare

set -e

# Get the repository root (parent of tools/bash/)
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

DO_CLEAN=false
DO_COMPARE=false
for arg in "$@"; do
    case "$arg" in
        clean) DO_CLEAN=true ;;
        compare) DO_COMPARE=true ;;
    esac
done

echo "=================================================="
echo "AUDIO BABEL PERFORMANCE BENCHMARKS"
echo "=================================================="
echo ""

# Clean if requested
if [[ "$DO_CLEAN" == true ]]; then
    echo "Cleaning build directory..."
    rm -rf build
fi

# Create build directory
if [[ ! -d "build" ]]; then
    echo "Creating build directory..."
    mkdir -p build
fi

# Configure with Release mode
echo "Configuring CMake (Release mode)..."
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..

# Build
echo ""
echo "Building performance benchmarks..."
JOBS=$(nproc 2>/dev/null || echo 4)
make performance_benchmarks -j "$JOBS"

# Check if executable exists
EXE_PATH="./performance_benchmarks"
if [[ ! -f "$EXE_PATH" ]]; then
    echo "ERROR: Performance benchmarks executable not found at: $EXE_PATH"
    exit 1
fi

# Run benchmarks
echo ""
echo "=================================================="
echo "Running benchmarks (this may take a few minutes)..."
echo "=================================================="
echo ""

"$EXE_PATH"

# Display results
echo ""
echo "=================================================="
echo "BENCHMARK RESULTS"
echo "=================================================="
echo ""

RESULTS_PATH="performance_results.txt"
if [[ -f "$RESULTS_PATH" ]]; then
    cat "$RESULTS_PATH"
    echo ""
    echo "Results saved to: build/$RESULTS_PATH"
else
    echo "WARNING: Results file not found at: $RESULTS_PATH"
fi

echo ""
echo "=================================================="
echo "Performance benchmarks completed successfully!"
echo "=================================================="

# Optionally compare against the committed baseline
if [[ "$DO_COMPARE" == true ]]; then
    echo ""
    echo "=================================================="
    echo "COMPARING AGAINST BASELINE"
    echo "=================================================="
    echo ""
    if command -v node >/dev/null 2>&1; then
        cd "$REPO_ROOT"
        node tools/node/compare-benchmarks.mjs || true
    else
        echo "NOTE: node not found on PATH; skipping baseline comparison."
        echo "Install Node.js and re-run with the 'compare' argument to compare against cpp/perf/baseline.json."
    fi
fi
