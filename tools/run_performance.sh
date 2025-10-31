#!/bin/bash
#
# Run performance benchmarks for the Audio Babel library.
# Builds in Release mode and executes the performance benchmark suite.
# Results are written to build/performance_results.txt.
#
# Usage:
#   ./run_performance.sh
#   ./run_performance.sh clean

set -e

# Get the repository root (parent of tools/)
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

echo "=================================================="
echo "AUDIO BABEL PERFORMANCE BENCHMARKS"
echo "=================================================="
echo ""

# Clean if requested
if [[ "${1:-}" == "clean" ]]; then
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
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..

# Build
echo ""
echo "Building performance benchmarks..."
mingw32-make performance_benchmarks -j 4

# Check if executable exists
EXE_PATH="./performance_benchmarks.exe"
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
