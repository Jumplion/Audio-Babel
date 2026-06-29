# `cpp/wasm/` — Emscripten WebAssembly build

Compiles the core library in [`cpp/src/`](../src/) to WebAssembly and exposes it
to the browser. Output goes to [`docs/wasm/`](../../docs/wasm/) (committed so the
GitHub Pages site needs no build step). The shipped WASM build is compiled with
`AUDIOBABEL_SCRAMBLE` enabled. See the [root README](../../README.md) for
one-time Emscripten setup and full build instructions.

## Files

| File | Purpose |
| ---- | ------- |
| `wasm_bindings.cpp` | Emscripten embind wrappers exposing the C++ API to JavaScript (see exported functions below). |
| `CMakeLists.txt` | Emscripten build config: `-O3 -flto` (Release) / `-O0 -g` (Debug), ES6 module output, browser-only, native WASM exceptions. |
| `build-wasm.sh` | Bash build script (run with the Emscripten environment activated). |
| `build-wasm.ps1` | PowerShell build script; supports `-Debug` and `-Clean`. |

## Exported functions

Consumed by [`docs/js/core/indexWasm.js`](../../docs/js/core/indexWasm.js):

| Function | Purpose |
| -------- | ------- |
| `getMetadata` | Extract metadata from a base64 index string. |
| `decodeIndex` | Decode an index to metadata, position, and PCM samples in one pass. |
| `encodeIndex` | Encode raw PCM bytes into a bijective base64 index string. |
| `reconstructIndex` | Reconstruct an index from a library position. |
| `getLibraryConstants` | Return library hierarchy constants as JSON. |
| `getGenreNames` / `getArtistNames` / `getAlbumNames` / `getTrackNames` | Batch cosmetic names for one sibling group at a time (see [`IndexNaming.h`](../include/IndexNaming.h)). |
