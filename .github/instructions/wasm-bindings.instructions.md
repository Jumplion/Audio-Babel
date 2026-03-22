---
description: "Use when editing WASM bindings, Emscripten build config, or exposing C++ functions to JavaScript."
applyTo: "cpp/wasm/**"
---
# WASM Binding Conventions

## Exposing functions to JS
- Register in the `EMSCRIPTEN_BINDINGS` block in `wasm_bindings.cpp`.
- Prefer `std::string` parameters over `const char*` to avoid `allow_raw_pointers()`.
- If raw pointers are unavoidable, use `allow_raw_pointers()` in the binding and add `EMSCRIPTEN_KEEPALIVE`.

## Returning strings/buffers to JS
- Allocate with `malloc(len + 1)`, copy with `strcpy`, return `char*`.
- For binary data, copy into a JS `Uint8Array` via `emscripten::val`, then `free()` the C++ allocation.
- See `reconstructAudioWrapper` and `getMetadataFromBase64` in `wasm_bindings.cpp` for patterns.

## Emscripten CMake flags
- Format: `-sFLAG=value` (no space after `-s`). Example: `-sWASM=1`, `-sALLOW_MEMORY_GROWTH=1`.
- Link flag for embind: `--bind` (set in `set_target_properties`).
- Output is copied to `docs/wasm/` via `add_custom_command` POST_BUILD.

## Common fix
- Missing Boost: `embuilder build boost_headers` (one-time, needs activated Emscripten).
