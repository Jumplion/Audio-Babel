# `docs/js/` — Web app JavaScript (ES6 modules)

Client-side logic for the static GitHub Pages site. All audio processing runs in
the browser via the WASM module in [`docs/wasm/`](../wasm/) — there is no
backend. See the [root README](../../README.md) for the site overview.

## Layout

| Folder | Role |
| ------ | ---- |
| [`core/`](core/) | WASM integration and the result-rendering pipeline. |
| [`pages/`](pages/) | Per-page entry points (`browse`, `search`, `main`) that wire UI to core. |
| [`ui/`](ui/) | Generic UI helpers (fragment loading, nav highlighting). |
| [`utils/`](utils/) | Pure, reusable helpers (base64, WAV, DOM, validation, error handling, metadata search). |

## Key principle

Metadata (genre/artist/album/track/position) is **always** produced by the real
C++/WASM calls — never fabricated client-side. The bijective base64 index *is*
the payload encoding; WAV headers exist only on materialized `.wav` blobs for
playback and download.
