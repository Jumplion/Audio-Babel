#!/bin/bash
# Build script for WebAssembly module
# Run this with Emscripten environment activated

set -e

echo "=== Building Audio Index WASM Module ==="

# Check if Emscripten is available
if ! command -v emcc &> /dev/null; then
    echo "❌ Emscripten not found!"
    echo "Please install Emscripten:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

echo "✓ Emscripten found: $(emcc --version | head -n1)"

# Directories
WASM_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$WASM_DIR/build"
OUTPUT_DIR="$WASM_DIR/../../audio-babel-record-store/public/wasm"

# Parse arguments
CONFIG_TYPE="Release"
CLEAN=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            CONFIG_TYPE="Debug"
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create directories
mkdir -p "$BUILD_DIR"
mkdir -p "$OUTPUT_DIR"

echo ""
echo "Configuring CMake..."
cd "$BUILD_DIR"

# Configure with Emscripten
emcmake cmake .. -DCMAKE_BUILD_TYPE=$CONFIG_TYPE

echo ""
echo "Building WASM module..."
emmake make -j$(nproc)

echo ""
echo "Copying files to output directory..."

# Copy WASM and JS files
if [ -f "audio-index.wasm" ]; then
    cp audio-index.wasm "$OUTPUT_DIR/"
    echo "✓ Copied audio-index.wasm"
else
    echo "❌ WASM file not found!"
    exit 1
fi

if [ -f "audio-index.js" ]; then
    cp audio-index.js "$OUTPUT_DIR/"
    echo "✓ Copied audio-index.js"
else
    echo "❌ JS file not found!"
    exit 1
fi

# Show file sizes
WASM_SIZE=$(du -h "audio-index.wasm" | cut -f1)
echo ""
echo "WASM module size: $WASM_SIZE"

echo ""
echo "=== Build Complete ==="
echo "WASM module available at: $OUTPUT_DIR"

cd "$WASM_DIR"
