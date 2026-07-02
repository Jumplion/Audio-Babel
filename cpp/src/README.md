# `cpp/src/` — Core library implementation (`namespace AudioBabel`)

Implementations of the headers in [`cpp/include/`](../include/). See that
folder's README for the public API and the index-format specification, and the
[root README](../../README.md) for the project overview.

## Files

| File | Implements | Purpose |
| ---- | ---------- | ------- |
| `FileIO.cpp` | `FileIO.h` | Reads and validates RIFF/WAVE files into `AudioData`, and writes `AudioData` back out as a standard WAV. Handles fmt-chunk variants, odd-sized/unknown chunks, and malformed input. |
| `Index.cpp` | `Index.h` | The PCM-payload ↔ big-integer bijection. Encodes the payload sample bytes as-is without endianness conversion (the `bitDepth`/`sampleRate` arguments only affect the cosmetic WAV header). Applies the optional scramble when `AUDIOBABEL_SCRAMBLE` is set. |
| `IndexScramble.cpp` | `IndexScramble.h` | The keyed, reversible content scramble (per-band Feistel) and length-spread involution, plus the band-index/repunit math used to recover an index's decoded length. |
| `LibraryPosition.cpp` | `LibraryPosition.h` | Computes a hierarchical library position from an index and reconstructs the index from a position. Uses a keyed within-room offset permutation (distinct salt from the other modules so keys never alias). |
| `IndexNaming.cpp` | `IndexNaming.h` | Derives the four metadata names from an index via an invertible big-integer Feistel permutation, and the inverse path that constructs indexes carrying requested names. |
| `IndexMetadata.cpp` | `IndexMetadata.h` | Assembles the metadata struct: name lookup, deterministic SVG cover generation, and library position. |

## Notes

- All modules key their permutations from **distinct salts** so the naming,
  scrambling, and positioning bijections can never alias one another.
- `Index` knows nothing about WAV headers or files — that boundary lives
  entirely in `FileIO`.
