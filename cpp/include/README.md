# `cpp/include/` — Public headers (`namespace AudioBabel`)

Declarations for the core C++ library. Implementations live in
[`cpp/src/`](../src/). For the project overview and the index format
specification see the [root README](../../README.md) and
[`docs/INDEX_FORMAT.md`](../../docs/INDEX_FORMAT.md).

## Files

| File | Purpose |
| ---- | ------- |
| `Constants.h` | Centralized compile-time constants: WAV/RIFF sizes, byte masks, and the bijective-index scheme parameters (`DEFAULT_BIT_DEPTH = 16`, `DEFAULT_SAMPLE_RATE = 44100`, `SAMPLE_ALPHABET_SIZE = B = 65536`). Kept in one header so every translation unit shares the same values. |
| `Utilities.h` | Header-only, inline helpers shared across the library: little-endian read/write, byte/bit helpers, base64 alphabet handling, `indexToB64`/`b64ToIndex`, `isValidBase64Url`, and Feistel mixing primitives. |
| `FileIO.h` | Owns the WAV file format. Declares the `AudioData` POD (header fields + PCM payload), WAV parsing/validation, and serialization. The only place sample rate, bit depth, and channel count are meaningful. |
| `Index.h` | The core PCM-payload ↔ big-integer bijection (`Index::encode`/`decode`). Payload-only, no header or metadata embedded. See the format section below. |
| `IndexScramble.h` | Optional, reversible keyed permutation (`AUDIOBABEL_SCRAMBLE` toggle) that scatters neighboring payloads and spreads short indices across a varied range of decoded durations, while keeping a perfect bijection. |
| `LibraryPosition.h` | Maps an index to a hierarchical "record shop" address (room / wall / shelf / album / track) and back. Bijective; 9,600 tracks per room. |
| `IndexNaming.h` | Deterministic genre/artist/album/track names derived from the index via an invertible Feistel permutation. Invertible, so desired names can be turned straight into indexes that carry them (no search). |
| `IndexMetadata.h` | Aggregates per-index metadata: the four `IndexNaming` labels, a 256×256 SVG cover derived from the index bytes, and the `LibraryPosition`. |

## The index format (payload-only bijection)

The index encodes **only** the PCM sample payload — no header, version, frame
count, sample rate, bit depth, or channel count. The atomic unit is one
unsigned little-endian 16-bit sample in `0..B-1` (`B = 65536`), and the integer
is built with bijective numeration (digit = value + 1):

- **Encode** (samples → integer): `n = 0;` then for each sample `v`: `n = n*B + (v+1)`
- **Decode** (integer → samples): `while n > 0: { n -= 1; v = n mod B; emit v; n = n/B }` then reverse

Because each digit is `value + 1`, trailing-zero (silence) samples are
preserved. The user-facing string is a bijective base-64 over the URL-safe
alphabet (`Utilities::indexToB64`). There is intentionally **no integrity
check**: every alphabet-valid index decodes to a valid payload. When decoding
back to a WAV, a fixed default header is applied (PCM, 44100 Hz, 16-bit, mono).
