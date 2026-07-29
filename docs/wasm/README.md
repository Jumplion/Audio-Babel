# `docs/wasm/` — Compiled WebAssembly artifacts

Build output of [`cpp/wasm/`](../../cpp/wasm/). **Do not edit by hand** —
rebuild from `cpp/wasm/` (see the [root README](../../README.md)). The shipped
build is compiled with `AUDIOBABEL_SCRAMBLE` enabled.

GitHub Pages deploys (`.github/workflows/static.yml`) rebuild this directory
from source on every deploy, so the live site is always current. The copy
checked into this directory is a convenience snapshot for browsing the site
locally without installing Emscripten — it is **not** auto-refreshed on every
push to `cpp/**` (that used to happen and was bloating repo history with a new
binary on every commit). Rebuild and commit it by hand when you want to update
the local snapshot, e.g. before a release.

## Files

| File | Purpose |
| ---- | ------- |
| `index.js` | Emscripten-generated ES6 module loader, consumed by [`../js/core/indexWasm.js`](../js/core/indexWasm.js). |
| `index.wasm` | The compiled WebAssembly binary (the core `AudioBabel` library). |
