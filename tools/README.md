# This folder contains helper scripts to build, run tests, run the example, and clean build artifacts

Files added

- `build.ps1` / `build.sh` - Configure and build the project.
- `run_tests.ps1` / `run_tests.sh` - Run the `tests_catch2.exe` from the `build` folder.
- `run_example.ps1` / `run_example.sh` - Run `example_main.exe` from the `build` folder.
- `clean.ps1` / `clean.sh` - Clean build artifacts. Pass `-RemoveDir` (PowerShell) or `--remove` (bash) to delete the build directory.

Usage (PowerShell)

Open PowerShell in the repository root and run, for example:

    .\tools\build.ps1 -Configuration Debug
    .\tools\run_tests.ps1

Usage (bash / WSL / msys)

    ./tools/build.sh Debug
    ./tools/run_tests.sh

Notes

- Scripts assume a `build` directory sibling to the `tools` folder (default `..\build`). Adjust parameters if your layout differs.
- `build.*` uses `MinGW Makefiles` by default to match the existing build commands in the repo. If you use Visual Studio or Ninja, pass another generator.
