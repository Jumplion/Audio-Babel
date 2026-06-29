# `docs/wasm/` — Compiled WebAssembly artifacts

Build output of [`cpp/wasm/`](../../cpp/wasm/), committed to the repo so the
GitHub Pages site works without a build step. **Do not edit by hand** — rebuild
from `cpp/wasm/` (see the [root README](../../README.md)). The shipped build is
compiled with `AUDIOBABEL_SCRAMBLE` enabled.

## Files

| File | Purpose |
| ---- | ------- |
| `index.js` | Emscripten-generated ES6 module loader, consumed by [`../js/core/indexWasm.js`](../js/core/indexWasm.js). |
| `index.wasm` | The compiled WebAssembly binary (the core `AudioBabel` library). |
