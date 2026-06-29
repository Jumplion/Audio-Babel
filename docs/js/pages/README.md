# `docs/js/pages/` — Per-page entry points

Each module wires a single HTML page's UI to the [`../core/`](../core/) pipeline
and [`../utils/`](../utils/) helpers. See [`../README.md`](../README.md) for the
overall JS layout.

## Files

| File | Page | Purpose |
| ---- | ---- | ------- |
| `search.js` | `search.html` | Reconstruct audio from a pasted index, generate a random index, or upload a WAV to derive its index. All three converge on the same `buildResultForIndex` render pipeline. The index is passed to/from WASM exactly as-is — no header added or stripped. |
| `main.js` | `search.html` | Entry point that wires up the reconstruct / generate-random / upload-WAV actions and metadata search. |
| `browse.js` | `browse.html` | Hierarchical navigation through the library (room → wall → shelf → album → track). Loads hierarchy constants from WASM at init (with C++ defaults as fallback) and caches sibling cosmetic names for breadcrumbs. |
