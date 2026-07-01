# `docs/js/core/` — WASM integration & result rendering

The bridge between the C++/WASM library in [`docs/wasm/`](../../wasm/) and the
page modules in [`../pages/`](../pages/). See [`../README.md`](../README.md) for
the overall JS layout.

## Files

| File | Purpose |
| ---- | ------- |
| `indexWasm.js` | `IndexWasm` class — loads the Emscripten ES6 module and provides a clean JS API over the exported C++ functions. |
| `wasmModule.js` | Lazy-loaded singleton accessor (`getWasmModule()`) so the WASM module is initialized once and shared across pages. |
| `resultDisplay.js` | Renders a reconstructed result: metadata, expandable text, clickable indexes, WaveSurfer playback, WAV download, and the on-demand "More Like This…" Similar Tracks list. Owns the WaveSurfer instance lifecycle. |
| `similarTracks.js` | Generates "Similar Tracks" — real indexes from transformed copies of the original PCM (jitter, silence, speed change), re-encoded via the WASM `encodeIndex` bijection (never fabricated). Invoked on demand by `resultDisplay.js`'s "More Like This…" button, not automatically on every result. |
| `indexViewer.js` | Renders a full index string as a standalone HTML page opened in a new browser tab. |
