## Speaker of Babel — AI assistant quick-start

Native C++ audio-indexing library with WebAssembly bindings for a serverless browser app (GitHub Pages). Audio is encoded as URL-safe base64 indexes (no padding) using `boost::multiprecision::cpp_int`.

### Repository layout

| Path | Purpose |
|------|---------|
| `cpp/include/`, `cpp/src/` | Core C++ library (`namespace AudioBabel`) |
| `cpp/tests/` | Catch2 v3 unit tests + legacy `TestRunner` (being phased out) |
| `cpp/wasm/` | Emscripten WASM build; outputs to `docs/wasm/` |
| `cpp/examples/`, `cpp/tools/` | CLI binaries (example, extract, reconstruct) |
| `docs/` | Static web app (ES6 modules, no server) |
| `tools/powershell/`, `tools/bash/` | Build/test/run scripts (PowerShell, Bash) |

### Build and test

```powershell
# Native (PowerShell — preferred on Windows)
& "${PWD}\tools\powershell\build.ps1" -Configuration Debug
& "${PWD}\tools\powershell\run_tests.ps1" -TestMode unit

# WASM (requires activated Emscripten: .\emsdk\emsdk_env.ps1)
cd cpp/wasm; .\build-wasm.ps1
```

Bash equivalents (from repo root): `tools/bash/build.sh Debug "Unix Makefiles" build`, `tools/bash/run_tests.sh build unit`, `cpp/wasm/build-wasm.sh`.

VS Code tasks are available: "Build (PowerShell)", "Run Tests (PowerShell)".

### C++ conventions

- **C++17**, `namespace AudioBabel`, utilities in `AudioBabel::Utilities`. Use fully-qualified names.
- **Error handling:** throw `std::invalid_argument`, not optionals.
- **Base64:** URL-safe alphabet (`A-Z a-z 0-9 - _`), no `=` padding. See `Utilities.h` for `isValidBase64Url` / `decodeBase64Url`.
- **Index serialization:** `cpp_int` ↔ bytes via `export_bits`. Keep both overloads of `extractMetadataFromIndex` symmetric.
- **Static helpers** stay file-local when they are implementation details.
- **New CLI targets:** follow the pattern in root `CMakeLists.txt` (`EXAMPLE_SRC`, `RECON_SRC`, `EXTRACT_SRC`).

### WASM conventions

- Bindings use Emscripten `embind` in `cpp/wasm/wasm_bindings.cpp`. Raw pointer args need `allow_raw_pointers()`.
- Emscripten flags: `-sFLAG=value` (no space). WASM link flag: `--bind`.
- If build fails with missing Boost: `embuilder build boost_headers`.

### Web app conventions

- ES6 modules throughout `docs/js/`. camelCase functions, `_prefixed` for private methods.
- WASM loaded via `docs/js/core/audioIndexWasm.js` (singleton pattern with lazy init).
- Audio constraints: WAV, 44.1 kHz, 16-bit mono, max 2 minutes.

### After making changes

- Run tests: `tools/powershell/run_tests.ps1 -TestMode unit`
- New tests use **Catch2** `REQUIRE`/`CHECK` macros (not legacy `RUN_CHECK`)
- Serialization changes → update both code paths **and** tests in `cpp/tests/`
