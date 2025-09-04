# Speaker of Babel — Build & Run

This repository contains a C++ audio-indexing library (`cpp/`), a small CLI example, unit tests, and a minimal web frontend + Node server under `web/`.

This README provides concise, actionable instructions for building and running the native C++ project and the web components on Windows (PowerShell) and for MSYS2/MinGW users.

## Native C++ (Windows PowerShell)

Prerequisites

- CMake (3.20+ recommended)
- A C++ toolchain (MSVC or MinGW)
- Git

Quick build (PowerShell)

```powershell
# From repository root
& "${PWD}\tools\build.ps1" -Configuration Debug
```

What this does

- Configures CMake in `build/` and builds the project
- Outputs binaries under `build/` (e.g. `example_main.exe`, `tests_runner.exe`)

Run the example CLI

```powershell
# Convert WAV -> index text
# Example with explicit paths:
.\build\example_main.exe .\cpp\examples\test.wav .\out\test_index.txt

# Reconstruct WAV from index
.\build\example_main.exe .\out\test_index.txt .\out\test_recon.wav
```

Run unit tests

```powershell
& "${PWD}\tools\run_tests.ps1"
```

Notes for MinGW / MSYS2 users

- If you prefer MinGW, open the MINGW64 shell and run the provided `tools/build.sh` script or run these commands:

```bash
pacman -Syu
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake
mkdir build; cd build
cmake -G "MinGW Makefiles" ..
mingw32-make -j4
```

## Web frontend and server (Node)

The web UI and a lightweight Node server live in `web/frontend` and `web/server`.

Prerequisites

- Node.js (18+ recommended) and npm

Start the server (development)

```powershell
cd web/server
npm install
npm start
```

Start the frontend (if applicable)

```powershell
cd web/frontend
npm install
# Start the frontend dev server if available (repo frontend may vary)
npm run dev
```

## Troubleshooting & tips

- Missing optional libraries (FFTW, GMP): the build will continue but some features or tests may be skipped. Install these via your OS package manager to enable them.
- If example binaries are not found in `build/`, re-run the build script and check CMake output for missing targets.
- Use the `tools/` scripts (`build.ps1`, `run_tests.ps1`, `run_example.ps1`) on Windows for consistent behavior across environments.
