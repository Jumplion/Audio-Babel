# `docs/js/utils/` — Reusable helpers

Pure, mostly side-effect-free helpers shared across pages. See
[`../README.md`](../README.md) for the overall JS layout.

## Files

| File | Purpose |
| ---- | ------- |
| `audioConstants.js` | Default audio format constants (`44100` / `16` / `1`), mirroring `cpp/include/Constants.h`. Single source of truth so magic numbers aren't scattered across modules. |
| `base64.js` | URL-safe base64 helpers using the same alphabet as the C++ library (`A–Z a–z 0–9 - _`, no padding), including room-number ↔ bijective-base64 conversion. |
| `wavUtils.js` | Shared WAV parsing (`parseWavFile`) and creation (`createWavFile`) so pages don't duplicate header logic. |
| `resultBuilder.js` | Builds the standardized result object consumed by `resultDisplay`. Returns raw PCM bytes (not a base64 WAV) so the WAV container is only built lazily on download. Metadata always from the WASM `getMetadata` call. |
| `metadataSearch.js` | Turns requested genre/artist/album/track names into concrete indexes via the invertible naming permutation (WASM `constructByNames`) — pinned fields fixed, free fields randomized. No literal search. |
| `findInLibrary.js` | Hands a library position from the Search page to the Browse page via `sessionStorage` (keeps it off the URL; self-clears once consumed). |
| `validationUtils.js` | Base64-URL input validation (`isValidBase64Url`) and filtering (`filterToBase64UrlChars`). Intentionally mirrors the C++ `Utilities::isValidBase64Url` at the UI boundary. |
| `dom.js` | Small DOM helpers: `escapeHtml` (XSS-safe) and `downloadBlob`. |
| `errorHandler.js` | Centralized user-facing error display (`showError`/`handleError`) and `ErrorLevel` severities for consistent messaging and logging. |
