## Speaker of Babel — AI assistant quick-start

This repository contains a native C++ audio-indexing library and small web UI/server. The notes below capture the essential, discoverable patterns and workflows an AI coding agent should know to be immediately productive.

### Big picture (where things live)
- `cpp/` — core C++ library and CLI examples. Header files in `cpp/include/`, implementation in `cpp/src/`, small example programs under `cpp/examples/`, and unit tests in `cpp/tests/`.
- `tools/` — convenience build/test/run scripts used on Windows (PowerShell) and *nix shells: `build.ps1`, `run_tests.ps1`, `run_example.ps1`, and their `*.sh` counterparts.
- `build/` — CMake-generated build artifacts and MSVC/MinGW outputs (binaries like `example_main.exe`, `tests_runner.exe`).
- `web/` — simple frontend and Node server (no native bindings): `web/frontend/` and `web/server/`.

### Quick developer workflows (explicit commands)
- Build on Windows PowerShell (recommended):
  - `& "${PWD}\tools\build.ps1" -Configuration Debug` (this runs CMake and builds targets into `build/`).
- Build on MinGW/MSYS2 or bash:
  - `./tools/build.sh Debug` or follow the sequence in `cpp/README.md` (cmake + `mingw32-make`).
- Run unit tests (PowerShell):
  - `& "${PWD}\tools\run_tests.ps1"` — test runner binary appears under `build/tests_runner.exe`.
- Run example CLI (after build):
  - `./build/example_main.exe <input> <output>` (see `cpp/README.md` for example commands).

Use the included VS Code tasks if available (labels like "Build (PowerShell)" and "Run Tests (PowerShell)") for convenience.

### Key project-specific conventions & patterns
- Index encoding: the project uses URL-safe base64 without padding (alphabet `A-Z a-z 0-9 - _`). See `cpp/src/IndexMetadata.cpp`:
  - Validation: `AudioBabel::isValidBase64Url` (in `cpp/include/Base64Url.h`) accepts only the URL-safe alphabet.
  - Decoding: `decodeUrlSafeBase64` (local helper) expects no `=` padding and throws `std::invalid_argument` on bad characters.
  - Encoding from bytes: `extractMetadataFromIndex(const cpp_int&)` builds a deterministic base64 string from raw bytes with the same URL-safe alphabet.
- Big-integer indexes: `boost::multiprecision::cpp_int` is used to represent indexes. Conversion to/from bytes uses `boost::multiprecision::export_bits` (see `IndexMetadata.cpp`). When changing index serialization, prefer to update both overloads of `extractMetadataFromIndex` so behavior remains symmetric.
- Metadata derivation: `buildMetadataFromBytesAndB64` (static helper in `IndexMetadata.cpp`) splits the base64 string into four fields (`genre`, `artist`, `album`, `track`) by computing weighted lengths from the raw byte sums. Empty input yields defaults `g0`, `a0`, `al0`, `t0`.
- SVG cover: `generateSvgCover` (in `IndexMetadata.cpp`) constructs a 256×256 SVG. The dominant color is derived from the first three bytes; the `track` string is embedded as centered white text. Cover data is stored in metadata as a `std::string` containing SVG text.

### Editing & tests guidance for AI agents
- Small C++ edits: prefer minimal, localized changes. The codebase uses plain C++ (C++11/C++14 style). Keep `static` helper functions local when they are implementation details.
- Error semantics: decoding functions throw exceptions (`std::invalid_argument`) rather than returning optional values. Keep or mirror this behavior when adding similar helpers.
- When modifying serialization/formatting (base64, export_bits, byte-order), update both code paths and existing unit tests in `cpp/tests/` and add tests if behavior changes.
- Run the project's tests after changes using `tools/run_tests.ps1` (PowerShell) or `tools/run_tests.sh` (bash). The CI/verification minimal smoke test is `build/tests_runner.exe`.

### Integration & external dependencies
- Native C++ depends on CMake and a C++ toolchain (MSVC or MinGW). Optional features may expect libraries like FFTW or GMP; if missing the build may skip related targets — see top-level `README.md` and `cpp/README.md` for notes.
- The web server is a separate Node project in `web/server/` and does not directly link the native library. Interaction is via generated index files and HTTP API between `web/frontend` and `web/server`.

### Files to inspect when working on features
- `cpp/src/IndexMetadata.cpp` — index serialization, base64 encode/decode, metadata and SVG cover generation.
- `cpp/include/IndexMetadata.h` — public API for metadata extraction.
- `tools/*` — scripts for consistent build/test/run across environments.
- `cpp/tests/` — unit tests and `test_main.cpp`.
- `web/server/README.md` and `web/frontend/` — where to find frontend/server behavior.

### Examples to copy when coding
- To validate index decoding behavior, mirror the `decodeUrlSafeBase64` alphabet and error handling from `cpp/src/IndexMetadata.cpp`.
- When adding a new CLI target, follow existing pattern in `tools/` and add a small wrapper binary under `cpp/examples/` with a matching entry in CMakeLists.

If anything in this document is unclear or you'd like more detail about a specific area (build internals, a particular source file, or tests), tell me which area and I'll expand it.
