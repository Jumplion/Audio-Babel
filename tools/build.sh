#!/usr/bin/env bash
set -euo pipefail

CONFIGURATION=${1:-Debug}
GENERATOR=${2:-"MinGW Makefiles"}
BUILD_DIR=${3:-../build}

mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR" >/dev/null

echo "[build.sh] Configuring with generator '$GENERATOR' and configuration '$CONFIGURATION' in $(pwd)"
cmake -G "$GENERATOR" -DCMAKE_BUILD_TYPE=$CONFIGURATION ..

if [[ "$GENERATOR" == *Makefiles* ]]; then
  JOBS=$(nproc 2>/dev/null || echo 1)
  echo "[build.sh] Building with make -j $JOBS"
  mingw32-make -j "$JOBS"
else
  echo "[build.sh] Building with 'cmake --build'"
  cmake --build . --config $CONFIGURATION
fi

popd >/dev/null
