# Speaker of Babel — Build & Run

This repository contains a C++ audio-indexing library (`cpp/`), a small CLI example, unit tests, and a web frontend under `docs/` (configured for GitHub Pages).

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

## Web frontend (GitHub Pages)

The web UI lives in `docs/` and is configured for GitHub Pages deployment.

To test locally:

```powershell
cd docs
python -m http.server 8080
```

Then open <http://localhost:8080> in your browser.

### GitHub Pages Setup

1. Go to your repository Settings → Pages
2. Under "Source", select "Deploy from a branch"
3. Under "Branch", select `main` and `/docs` folder
4. Click "Save"

Your site will be available at `https://[username].github.io/[repository-name]/`

## Troubleshooting & tips

- Missing optional libraries (FFTW, GMP): the build will continue but some features or tests may be skipped. Install these via your OS package manager to enable them.
- If example binaries are not found in `build/`, re-run the build script and check CMake output for missing targets.
- Use the `tools/` scripts (`build.ps1`, `run_tests.ps1`, `run_example.ps1`) on Windows for consistent behavior across environments.
