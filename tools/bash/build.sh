#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: build.sh [CONFIGURATION] [GENERATOR] [BUILD_DIR]

  CONFIGURATION  Debug | Release | RelWithDebInfo | MinSizeRel   (default: Debug)
  GENERATOR      CMake generator string, e.g.:
                   "Unix Makefiles"   (default — native Linux/WSL/macOS)
                   "MinGW Makefiles"  (native Windows with MinGW)
                   "Ninja"
  BUILD_DIR      Path to the build directory                    (default: ../build)

Examples:
  ./build.sh                                 # Debug, Unix Makefiles, ../build
  ./build.sh Release
  ./build.sh Debug "Unix Makefiles" build     # run from the repo root
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

CONFIGURATION=${1:-Debug}
GENERATOR=${2:-"Unix Makefiles"}
BUILD_DIR=${3:-../build}

mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR" >/dev/null

echo "[build.sh] Configuring with generator '$GENERATOR' and configuration '$CONFIGURATION' in $(pwd)"
cmake -G "$GENERATOR" -DCMAKE_BUILD_TYPE=$CONFIGURATION ..

if [[ "$GENERATOR" == "MinGW Makefiles" ]]; then
  JOBS=$(nproc 2>/dev/null || echo 1)
  echo "[build.sh] Building with mingw32-make -j $JOBS"
  mingw32-make -j "$JOBS"
elif [[ "$GENERATOR" == *Makefiles* ]]; then
  JOBS=$(nproc 2>/dev/null || echo 1)
  echo "[build.sh] Building with make -j $JOBS"
  make -j "$JOBS"
else
  echo "[build.sh] Building with 'cmake --build'"
  cmake --build . --config $CONFIGURATION
fi

popd >/dev/null
