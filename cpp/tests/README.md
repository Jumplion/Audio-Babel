# `cpp/tests/` — Unit & performance tests (Catch2 v3)

Tests for the core library in [`cpp/src/`](../src/). Built by
[`CMakeLists.txt`](../../CMakeLists.txt) into `tests_catch2` (unit) and
`performance_benchmarks` (benchmarks). See the [root README](../../README.md)
for build and run instructions.

## Files

| File | Covers |
| ---- | ------ |
| `test_common.h` | Shared, framework-free test helpers (temp-path generation, RAII `TempFile`). No external dependencies. |
| `test_bijection.cpp` | The payload-only true bijection: `decode(encode(x)) == x` (incl. leading/trailing silence and exact count), `b64ToIndex`/`indexToB64` round-trips, trailing-zero sensitivity, and WAV → index → WAV byte exactness. |
| `test_index.cpp` | The `Index` class: `encode`/`decode` round-trips, edge cases, and the `FileIO` helpers used to build payloads alongside it. |
| `test_base64.cpp` | `Utilities::isValidBase64Url` and the validation performed at the metadata-extraction boundary. |
| `test_wav_parsing.cpp` | WAV parsing/export: header correctness, fmt-chunk variants, odd-sized unknown chunks, truncated/malformed files, unsupported bit depths, oversized declared data chunks. |
| `test_metadata.cpp` | `IndexMetadata` extraction from integers and base64 strings, SVG cover generation, field validation/determinism, malformed input. |
| `test_index_naming.cpp` | `IndexNaming`: deterministic names, neighbour divergence, name→index inversion (no search), partial-query variety, and name width limits. |
| `test_library_position.cpp` | `LibraryPosition` calculation/reconstruction, bijection, boundary values, constants, uniqueness, and neighbour dissimilarity. |
| `test_scramble.cpp` | The optional scramble: pure transform (bijection, inverse, 0→0, neighbour scatter), the length-spread, and the encode/decode toggle wiring. |
| `test_integration.cpp` | End-to-end pipeline: read WAV → index → reconstruct WAV. |
| `test_performance.cpp` | Benchmarks (throughput/latency) for key operations; run in Release for meaningful numbers. |

## `Test Audio/`

WAV fixtures used by the parsing and integration tests (sine/square/triangle/
sawtooth tones, noise, plucks, and a tempo clip).
