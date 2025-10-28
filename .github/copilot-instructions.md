## Speaker of Babel — AI assistant quick-start

This repository contains a native C++ audio-indexing library with WebAssembly bindings for serverless browser deployment. The notes below capture the essential, discoverable patterns and workflows an AI coding agent should know to be immediately productive.

### Big picture (where things live)
- `cpp/` — core C++ library (`include/`, `src/`), CLI examples (`examples/`), unit tests (`tests/`), and tools (`tools/` contains extract/reconstruct CLIs).
- `cpp/wasm/` — WebAssembly build system: `CMakeLists.txt`, `wasm_bindings.cpp`, and build scripts (`build-wasm.ps1`, `build-wasm.sh`). Outputs to `docs/wasm/`.
- `docs/` — serverless web app for GitHub Pages. Uses WASM module for client-side index generation. Contains HTML/CSS/JS files and documentation.
- `tools/` — convenience build/test/run scripts for native builds on Windows (PowerShell) and *nix shells: `build.ps1`, `run_tests.ps1`, `run_example.ps1`, and their `*.sh` counterparts.
- `build/` — CMake-generated native build artifacts (binaries like `example_main.exe`, `tests_runner.exe`). Separate from `cpp/wasm/build/`.
- `emsdk/` — Emscripten SDK installation (not tracked in git, installed locally).

### Quick developer workflows (explicit commands)

**Native C++ builds:**
- Build native library (PowerShell): `& "${PWD}\tools\build.ps1" -Configuration Debug` — runs CMake and builds targets into `build/`.
- Build native library (bash/MinGW): `./tools/build.sh Debug` — or follow the sequence in `cpp/README.md` (cmake + `mingw32-make`).
- Run unit tests (PowerShell): `& "${PWD}\tools\run_tests.ps1"` — runs `build/tests_runner.exe`.
- Run example CLI: `./build/example_main.exe <input> <output>` (see `cpp/README.md` for usage).

**WebAssembly builds (requires Emscripten):**
- **Prerequisites**: Emscripten SDK must be installed at repo root (`emsdk/`) and activated with `.\emsdk\emsdk_env.ps1` (Windows) or `source ./emsdk/emsdk_env.sh` (*nix).
- **Install Boost headers for WASM**: `embuilder build boost_headers` (only needed once, installs to Emscripten cache).
- **Build WASM module** (PowerShell): `cd cpp/wasm; .\build-wasm.ps1` — outputs `audio-index.wasm` and `audio-index.js` to `docs/wasm/`.
- **Build WASM module** (bash): `cd cpp/wasm; ./build-wasm.sh` — same output location.
- **Clean WASM build**: Add `-Clean` flag to build scripts.

**Important**: WASM builds require Emscripten environment activated in the current shell session. Native builds do not require Emscripten.

Use the included VS Code tasks if available (labels like "Build (PowerShell)", "Run Tests (PowerShell)") for convenience.

### Key project-specific conventions & patterns

**C++ namespace and organization:**
- All core library code lives in `namespace AudioBabel` (see `cpp/include/*.h`).
- Utilities are in nested namespace `AudioBabel::Utilities` (`cpp/include/Utilities.h`) — header-only helpers for endian conversion, base64, and byte manipulation.
- When writing C++ code, always use fully-qualified names like `AudioBabel::Utilities::decodeBase64Url` to avoid ambiguity.

**Index encoding:**
- The project uses URL-safe base64 **without padding** (alphabet `A-Z a-z 0-9 - _`). See `cpp/src/IndexMetadata.cpp`:
  - Validation: `AudioBabel::Utilities::isValidBase64Url` (in `cpp/include/Utilities.h`) accepts only the URL-safe alphabet, rejects `=` padding.
  - Decoding: `AudioBabel::Utilities::decodeBase64Url` expects no `=` padding and throws `std::invalid_argument` on bad characters.
  - Encoding from bytes: `extractMetadataFromIndex(const cpp_int&)` builds a deterministic base64 string from raw bytes with the same URL-safe alphabet.

**Big-integer indexes:**
- `boost::multiprecision::cpp_int` is used to represent indexes. Conversion to/from bytes uses `boost::multiprecision::export_bits` (see `IndexMetadata.cpp`).
- When changing index serialization, update both overloads of `extractMetadataFromIndex` so behavior remains symmetric.

**Metadata derivation:**
- `buildMetadataFromBytesAndB64` (static helper in `IndexMetadata.cpp`) splits the base64 string into four fields (`genre`, `artist`, `album`, `track`) by computing weighted lengths from the raw byte sums. Empty input yields defaults `g0`, `a0`, `al0`, `t0`.

**SVG cover generation:**
- `generateSvgCover` (in `IndexMetadata.cpp`) constructs a 256×256 SVG. The dominant color is derived from the first three bytes; the `track` string is embedded as centered white text. Cover data is stored in metadata as a `std::string` containing SVG text.

**Audio constraints (WASM/web interface):**
- Maximum duration: 2 minutes (120 seconds)
- Format: WAV, 44.1kHz, 16-bit, mono
- These constraints ensure manageable index sizes (~14 MB base64 encoded) for browser usage.

### Editing & tests guidance for AI agents

**C++ coding style:**
- The codebase uses C++17. Keep `static` helper functions local when they are implementation details.
- Error semantics: decoding functions throw exceptions (`std::invalid_argument`) rather than returning optional values. Keep or mirror this behavior when adding similar helpers.
- When modifying serialization/formatting (base64, export_bits, byte-order), update both code paths and existing unit tests in `cpp/tests/` and add tests if behavior changes.
- Run tests after changes: `tools/run_tests.ps1` (PowerShell) or `tools/run_tests.sh` (bash).

**WASM-specific considerations:**
- WASM bindings in `cpp/wasm/wasm_bindings.cpp` use Emscripten's `embind` API (see `#include <emscripten/bind.h>`).
- Functions taking raw pointers (`const char*`) must be wrapped with `allow_raw_pointers()` in `EMSCRIPTEN_BINDINGS` block (see line 194 in wasm_bindings.cpp).
- Emscripten compiler flags in `cpp/wasm/CMakeLists.txt` use `-sFLAG=value` format (no space after `-s`). Example: `-sWASM=1`, `-sALLOW_MEMORY_GROWTH=1`.
- WASM build outputs two files: `audio-index.wasm` (binary) and `audio-index.js` (glue code). Both are copied to `docs/wasm/` automatically.
- Memory limits: WASM module configured with 64MB initial, 2GB max (see `INITIAL_MEMORY` and `MAXIMUM_MEMORY` in CMakeLists.txt).
- **Critical**: If WASM build fails with "undefined symbol: _embind_register_*" errors, ensure `--bind` link flag is set in `set_target_properties` (see cpp/wasm/CMakeLists.txt line 68-71).

### Integration & external dependencies

**Native C++ dependencies:**
- CMake (3.15+) and C++17 toolchain (MSVC or MinGW on Windows, GCC/Clang on *nix).
- **Boost.Multiprecision** (header-only): Required for `cpp_int` big integer support. Native builds look for Boost in `C:/vcpkg/installed/x64-mingw-dynamic/include` or `C:/msys64/mingw64/include`. WASM builds use Emscripten's Boost (installed via `embuilder build boost_headers`).
- Optional: FFTW3, GMP (if missing, build may skip related targets).

**WASM-specific dependencies:**
- **Emscripten SDK**: Install to repo root `emsdk/` directory. Activate with `.\emsdk\emsdk_env.ps1` (Windows) or `source ./emsdk/emsdk_env.sh` (*nix) before building WASM.
- **Boost headers for Emscripten**: Run `embuilder build boost_headers` once after installing Emscripten. This downloads and caches Boost headers (~2-3 minutes).
- **MinGW make**: WASM build scripts use `mingw32-make` on Windows (part of MSYS2 MinGW64 toolchain).

**Web application:**
- The web app in `docs/` is serverless (GitHub Pages). It loads the WASM module (`wasm/audio-index.wasm`) in the browser for client-side index generation.
- No server-side Node.js required for deployment (fully static). JavaScript wrapper at `js/audioIndexWasm.js` provides clean API for WASM module.

**Common dependency issues:**
- If native build fails with "Boost.Multiprecision header not found", install Boost via vcpkg or MSYS2 (`pacman -S mingw-w64-x86_64-boost`).
- If WASM build fails with "boost/multiprecision/cpp_int.hpp not found", run `embuilder build boost_headers` to install Boost for Emscripten.
- If WASM build fails with path-with-spaces errors, the CMakeLists.txt builds to local directory first, then copies files (see `RUNTIME_OUTPUT_DIRECTORY` and `add_custom_command` in cpp/wasm/CMakeLists.txt).

### Files to inspect when working on features

**Core C++ library:**
- `cpp/src/IndexMetadata.cpp` — index serialization, base64 encode/decode, metadata and SVG cover generation.
- `cpp/include/IndexMetadata.h` — public API for metadata extraction.
- `cpp/include/Utilities.h` — header-only utilities (base64, endian conversion, byte manipulation). Used throughout the codebase.
- `cpp/include/Constants.h` — audio format constants (sample rate, bit depth, etc.).

**WASM integration:**
- `cpp/wasm/wasm_bindings.cpp` — Emscripten bindings exposing C++ functions to JavaScript (uses embind API).
- `cpp/wasm/CMakeLists.txt` — WASM build configuration (Emscripten flags, output paths, Boost detection).
- `cpp/wasm/build-wasm.ps1` / `build-wasm.sh` — convenience build scripts with Emscripten environment checks.
- `docs/js/audioIndexWasm.js` — JavaScript wrapper providing clean API for WASM module.

**Web application:**
- `docs/` — HTML/CSS/JS for GitHub Pages deployment.
- `docs/js/` — JavaScript modules for audio processing, UI, and WASM integration.

**Build infrastructure:**
- `CMakeLists.txt` (root) — native build configuration for library and CLI tools.
- `tools/*` — scripts for consistent native build/test/run across environments.
- `cpp/tests/` — unit tests and `test_main.cpp`.

### Examples to copy when coding

**C++ patterns:**
- To validate index decoding behavior, mirror the `AudioBabel::Utilities::decodeBase64Url` alphabet and error handling from `cpp/include/Utilities.h`.
- When adding a new CLI target, follow existing pattern in root `CMakeLists.txt` (see `EXAMPLE_SRC`, `TEST_SRC`, `RECON_SRC`, `EXTRACT_SRC`) and add a small wrapper binary under `cpp/examples/` or `cpp/tools/`.
- For big-integer serialization, see `export_bits` usage in `IndexMetadata.cpp` lines that convert `cpp_int` to `std::vector<uint8_t>`.

**WASM patterns:**
- To expose a new C++ function to JavaScript, add it to `EMSCRIPTEN_BINDINGS` block in `cpp/wasm/wasm_bindings.cpp` (line ~188). Use `allow_raw_pointers()` if function takes `const char*` arguments.
- Memory management example: see `getMetadataFromBase64` in wasm_bindings.cpp (uses `malloc`/`strcpy` for returning strings to JavaScript).

**Web application patterns:**
- Position encoding: see `docs/js/positionEncoder.js` for mapping base64 indexes to Room/Wall/Shelf/Track hierarchy.
- Audio processing: see `docs/js/audioIndexWasm.js` for WASM integration and WAV file generation.

### Troubleshooting common issues

**"WASM build fails with 'boost/multiprecision/cpp_int.hpp not found'"**
- Solution: Run `embuilder build boost_headers` in a shell with Emscripten activated. This installs Boost to Emscripten's cache (takes 2-3 minutes).

**"WASM build fails with 'undefined symbol: _embind_register_bindings'"**
- Solution: Ensure `--bind` link flag is set in `cpp/wasm/CMakeLists.txt` (see `set_target_properties` with `LINK_FLAGS "--bind"`).

**"Native build can't find Boost headers"**
- Solution: Install Boost via MSYS2 (`pacman -S mingw-w64-x86_64-boost`) or vcpkg, or set `BOOST_MP_INCLUDE_DIR` CMake variable to your Boost installation.

**"PowerShell script errors about execution policy"**
- Solution: Run `Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass` in PowerShell before running build scripts.

If anything in this document is unclear or you'd like more detail about a specific area (WASM integration, build internals, a particular source file, or tests), tell me which area and I'll expand it.
