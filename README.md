# Speaker of Babel

A deterministic audio-indexing library that encodes entire audio files as URL-safe base64 strings, enabling lossless reconstruction from a single "index." Inspired by Jorge Luis Borges' *Library of Babel*, this project creates an infinite, navigable collection of all possible audio files within given parameters.

Every unique audio file maps to exactly one index, and every index maps to exactly one audio file — a perfect bijection between integers and sound.

**Live site:** The web app is hosted on GitHub Pages as a serverless, static site. All audio processing happens client-side via WebAssembly.

## Technology

| Layer | Stack |
| ------- | ------- |
| Core library | C++17, Boost.Multiprecision (`cpp_int`) |
| Testing | Catch2 v3 |
| WebAssembly | Emscripten (embind) |
| Web app | ES6 modules, WaveSurfer.js |
| Build | CMake, MinGW (native), Emscripten (WASM) |

## Repository Layout

| Path | Purpose |
| ------ | --------- |
| `cpp/include/`, `cpp/src/` | Core C++ library (`namespace AudioBabel`) |
| `cpp/tests/` | Catch2 unit tests |
| `cpp/wasm/` | Emscripten WASM build; outputs to `docs/wasm/` |
| `docs/` | Static web app (GitHub Pages) |
| `tools/powershell/`, `tools/bash/` | Build/test/run scripts (PowerShell, Bash) |

## How an Index Is Generated from Audio

An audio index is a single big integer that **is a true bijection of the PCM
sample payload** — every payload maps to exactly one index and every index maps
to exactly one payload. The index contains **no header** and no format metadata;
it is serialized as a URL-safe base64 string for storage and transmission. See
[`docs/INDEX_FORMAT.md`](docs/INDEX_FORMAT.md) for the full specification.

### Step 1 — Parse the WAV file

The input WAV file is read and validated (RIFF header, format chunk, PCM data chunk). The raw PCM sample bytes are extracted into an `AudioData` struct.

### Step 2 — Index the samples (payload only)

The payload is read as a sequence of **unsigned little-endian 16-bit samples**
(alphabet size `B = 1u << DEFAULT_BIT_DEPTH = 65536`). The integer is built with
**bijective numeration**, where each digit is the sample value plus one:

```cpp
n = 0;
for each sample v in order:   // v in 0..65535
    n = n * B + (v + 1);
```

Because each digit is `value + 1`, trailing zero (silence) samples are preserved:
`k` vs `k+1` trailing zeros yield different indices. No header, version, frame
count, sample rate, bit depth, or channel count is stored in the index.

### Step 3 — Encode as bijective URL-safe base64

The big integer is rendered as a string over the URL-safe alphabet
(`A–Z a–z 0–9 - _`, no padding) using the same bijective numeration
(`Utilities::indexToB64`). The empty payload maps to integer `0`, which maps to
the empty string.

## How Audio Is Reconstructed from an Index

### Step 1 — Decode the index string to a big integer

The URL-safe base64 string is converted back to a `cpp_int` with
`Utilities::b64ToIndex` (`n = n * 64 + (alphaValue(c) + 1)` per character). Every
alphabet-valid string decodes; there is intentionally **no integrity check**, so
a truncated or mistyped index simply decodes to a different valid payload.

### Step 2 — Decode the integer back to samples

The inverse bijection recovers the samples exactly, including leading and
trailing zero samples and the exact sample count:

```cpp
while (n > 0) {
    n -= 1;
    v  = n mod B;   // emit v
    n  = n / B;
}                   // then reverse; serialize each sample little-endian
```

### Step 3 — Apply the fixed default header and write a WAV

The decoded samples are wrapped in a fixed default header — **PCM, 44100 Hz,
16-bit, mono** — and written as a standard RIFF/WAVE file. The header is applied
only at this step; it is never part of the index.

## Library Position

Every index has a unique position in a hierarchical "library":

| Level | Count |
| ------- | ------- |
| Walls per room | 4 |
| Shelves per wall | 5 |
| Albums per shelf | 32 |
| Tracks per album | 15 |
| **Items per room** | **9,600** |

The index integer is divided by 9,600. The quotient identifies the room (base64-encoded), and the remainder is decomposed into wall → shelf → album → track. This gives every index a browsable, human-friendly address and provides a perfect bijection between index values and library positions. See `cpp/include/LibraryPosition.h`.

## Index Metadata

`IndexMetadata::extractMetadataFromIndex` derives a genre, artist, album, and track label (URL-safe base64 strings, weighted by the index's byte sums) plus a 256×256 SVG album cover, all deterministically from the index bytes — no extra data is stored. See `cpp/include/IndexMetadata.h`.

## Optional Index Scrambling

By default, similar payloads land at numerically nearby indices, and because the index uses bijective base-65536 numeration, any short index (every browseable library position, or a casually-typed one) decodes to only one or two samples of near-silence. A reversible scramble fixes this with two keyed bijections: a per-band Feistel permutation that scatters neighbors' contents, and a length-spread involution that maps short indices onto a wide range of distinct, log-spaced durations (100 ms to 15 s by default) — so browsing consecutive positions yields clips that vary widely in both length and content, while the index↔payload mapping stays a perfect bijection. It's a compile-time toggle (`AUDIOBABEL_SCRAMBLE`), **enabled in the shipped WASM build**; see [`docs/INDEX_FORMAT.md`](docs/INDEX_FORMAT.md) for the full algorithm.

## Building the Project

### Prerequisites

- **CMake** 3.14+
- **MinGW-w64** (Windows) or **GCC** (Linux) with C++17
- **Boost** headers (Multiprecision) — fetched automatically on some setups, or install via your package manager
- **Emscripten SDK** (for WASM builds only)

### Native Build

**PowerShell (Windows):**

```powershell
.\tools\powershell\build.ps1 -Configuration Debug
```

**Bash (Linux / WSL):**

```bash
./tools/bash/build.sh Debug "Unix Makefiles" build
```

This creates the `build/` directory and compiles:

| Output | Description |
| -------- | ------------- |
| `build/audiolib.lib` or `build/libaudiolib.a` (toolchain-dependent) | Static library |
| `build/tests_catch2.exe` (Windows) / `build/tests_catch2` (Linux) | Unit tests |
| `build/performance_benchmarks.exe` (Windows) / `build/performance_benchmarks` (Linux) | Performance benchmarks |

### Running Tests

```powershell
.\tools\powershell\run_tests.ps1   # PowerShell
./tools/bash/run_tests.sh build unit   # Bash
```

Tests use Catch2 v3 and cover base64 encoding/decoding, index generation/reconstruction, WAV parsing, metadata extraction, library position calculation, and end-to-end integration.

### Manual CMake Build

```powershell
mkdir build; cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
mingw32-make -j4
```

```bash
mkdir build && cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
make -j4
```

### Cleaning Build Artifacts

```powershell
.\tools\powershell\clean.ps1              # Clean build artifacts
.\tools\powershell\clean.ps1 -RemoveDir   # Delete the build/ directory entirely
```

```bash
./tools/bash/clean.sh build              # Clean build artifacts
./tools/bash/clean.sh build --remove     # Delete the build/ directory entirely
```

## Building the WASM Module

### One-Time Setup

Install the Emscripten SDK inside `cpp/wasm/`:

```powershell
cd cpp/wasm
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
.\emsdk install latest
.\emsdk activate latest
```

If the build fails with missing Boost headers:

```powershell
embuilder build boost_headers
```

### Activate Emscripten

Before every WASM build, activate the SDK in your current shell:

```powershell
.\cpp\wasm\emsdk\emsdk_env.ps1     # PowerShell
source ./cpp/wasm/emsdk/emsdk_env.sh  # Bash
```

### Build

```powershell
cd cpp/wasm
.\build-wasm.ps1            # Release build (default)
.\build-wasm.ps1 -Debug     # Debug build
.\build-wasm.ps1 -Clean     # Clean build directory first
```

Or with Bash:

```bash
cd cpp/wasm
./build-wasm.sh
```

The build compiles `wasm_bindings.cpp` (Emscripten embind) and outputs to `docs/wasm/`:

| Output | Description |
| -------- | ------------- |
| `docs/wasm/index.js` | ES6 module loader |
| `docs/wasm/index.wasm` | Compiled WebAssembly binary |

These files are committed to the repo so the GitHub Pages site works without a build step.

### WASM Build Configuration

- **Optimization:** `-O3` (Release) / `-O0 -g` (Debug)
- **Memory:** 64 MB initial, 2 GB max
- **Module format:** ES6 (`-sMODULARIZE`, `-sEXPORT_ES6`)
- **Bindings:** Emscripten embind (`--bind`)
- **Exceptions:** Enabled (`-fexceptions`)

## Web App

The `docs/` directory is a static site served by GitHub Pages. All audio processing runs in the browser via WASM — there is no backend.

To preview it locally:

```powershell
.\tools\powershell\serve-docs.ps1            # serves docs/ on http://localhost:3000
```

```bash
./tools/bash/serve-docs.sh                   # Linux / WSL / msys, serves docs/ on http://localhost:3000
```

| Page | Purpose |
| ------ | --------- |
| `index.html` | Home — overview and glossary |
| `browse.html` | Navigate the library by room / wall / shelf / album / track |
| `search.html` | Reconstruct audio from a typed index, generate a random index, or upload a WAV to extract its index |

### Audio Constraints

| Parameter | Value |
| ----------- | ------- |
| Format | WAV (PCM) |
| Sample rate | 44,100 Hz |
| Bit depth | 16-bit |
| Channels | Mono |

## License

See repository for license details.
