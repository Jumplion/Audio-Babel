# This folder contains helper scripts to build, run tests, run the example, and clean build artifacts

Scripts are split by shell:

- `powershell/` - PowerShell (`.ps1`) scripts, for native Windows builds.
- `bash/` - Bash (`.sh`) scripts, for Linux / WSL / msys builds.

Files added

- `build.ps1` / `build.sh` - Configure and build the project.
- `run_tests.ps1` / `run_tests.sh` - Run the `tests_catch2` binary from the `build` folder.
- `run_example.ps1` / `run_example.sh` - Run the `example_main` binary from the `build` folder.
- `clean.ps1` / `clean.sh` - Clean build artifacts. Pass `-RemoveDir` (PowerShell) or `--remove` (bash) to delete the build directory.
- `run-clang-tidy.ps1` / `run-clang-tidy.sh` - Run clang-tidy (and clang-format on PowerShell) over the C++ sources.
- `run_performance.ps1` / `run_performance.sh` - Build (Release) and run the performance benchmark suite.
- `npm-serve.ps1` - Serve the web frontend (Windows only; no bash equivalent).

Usage (PowerShell)

Open PowerShell in the repository root and run, for example:

    .\tools\powershell\build.ps1 -Configuration Debug
    .\tools\powershell\run_tests.ps1

Usage (bash / WSL / msys)

Open a shell in the repository root and run, for example:

    ./tools/bash/build.sh Debug "Unix Makefiles" build
    ./tools/bash/run_tests.sh build unit

Notes

- `build.sh` / `clean.sh` / `run_example.sh` / `run_tests.sh` resolve `BUILD_DIR` (default `../build`) relative to the current working directory, not the script location. The default only resolves to the repo-root `build/` folder when invoked from a directory one level below the repo root (e.g. `cpp/`); when invoking from the repo root, pass `build` explicitly as shown above. `run_performance.sh` and `run-clang-tidy.sh` resolve paths relative to the script itself and always target the repo-root `build/` folder regardless of working directory.
- `build.sh` / `run_performance.sh` default to the `Unix Makefiles` generator and `make`, matching a native Linux/WSL toolchain (no MinGW). `build.ps1` / `run_performance.ps1` default to `MinGW Makefiles` and `mingw32-make` for native Windows builds. Pass another generator if you use Visual Studio, Ninja, etc.
