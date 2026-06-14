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
| `cpp/examples/`, `cpp/tools/` | CLI binaries (example, extract, reconstruct) |
| `docs/` | Static web app (GitHub Pages) |
| `tools/` | Cross-platform build/test/run scripts |

## How an Index Is Generated from Audio

An audio index is a single big integer that embeds both format metadata and the complete PCM audio payload. It is serialized as a URL-safe base64 string for storage and transmission.

### Step 1 — Parse the WAV file

The input WAV file is read and validated (RIFF header, format chunk, PCM data chunk). The raw PCM sample bytes and format properties are extracted into an `AudioData` struct containing the sample rate, bit depth, channel count, frame count, and sample bytes.

### Step 2 — Build the 13-byte header

A fixed-size header is constructed with the audio's format metadata:

```cpp
Byte 0:       0x01            (format version)
Bytes 1–4:    frame count     (uint32, little-endian)
Bytes 5–8:    sample rate     (uint32, little-endian)
Bytes 9–10:   bit depth       (uint16, little-endian — 8, 16, or 32)
Bytes 11–12:  channel count   (uint16, little-endian)
```

For example, 2 minutes of 44.1 kHz / 16-bit / mono audio has 5,292,000 frames and a 10,584,000-byte PCM payload.

### Step 3 — Concatenate header + PCM data

The 13-byte header and the raw PCM sample bytes are joined into a single byte array:

```cpp
[13-byte header] [PCM sample bytes (little-endian per sample)]
```

### Step 4 — Convert to a big integer

The byte array is imported into a `boost::multiprecision::cpp_int` using MSB-first ordering — byte 0 (the version byte) becomes the most significant byte, and the last PCM byte becomes the least significant:

```cpp
boost::multiprecision::import_bits(index, bytes.begin(), bytes.end(), 8, true);
```

### Step 5 — Encode as URL-safe base64

The big integer is exported back to bytes and then encoded using the URL-safe base64 alphabet (`A–Z a–z 0–9 - _`) with **no padding** characters. The result is a single continuous string like `AQMAAABELEGAABAACAA...` that uniquely identifies and fully contains the audio file.

## How Audio Is Reconstructed from an Index

### Step 1 — Decode base64 to bytes

The URL-safe base64 string (no padding) is decoded back into a byte array. Every character is validated against the allowed alphabet (`A–Z a–z 0–9 - _`).

### Step 2 — Import bytes into a big integer

The byte array is loaded into a `cpp_int` via `import_bits` (MSB-first).

### Step 3 — Export the big integer back to bytes

`export_bits` serializes the integer back to a byte vector. Because `export_bits` strips leading zeros that may have been trailing in the original payload, the header metadata is used to restore the expected length.

### Step 4 — Parse the 13-byte header

The first 13 bytes are read to recover format metadata: version (must be `0x01`), frame count, sample rate, bit depth, and channel count. These values are validated (e.g., bit depth must be 8, 16, or 32).

### Step 5 — Restore trailing zero bytes

The expected PCM payload size is computed from the header:

```cpp
expected_bytes = frame_count × channel_count × (bit_depth / 8)
```

If the actual payload is shorter (because big-integer export strips trailing zeros), zero bytes are appended until the expected size is reached.

### Step 6 — Extract PCM samples

Bytes 13 onward are sliced as the raw PCM audio data. Combined with the header metadata, this yields a complete `AudioData` struct ready for playback or WAV export.

### Step 7 — Write a WAV file (optional)

The `AudioData` is wrapped in a standard RIFF/WAVE container (44-byte header + PCM payload) and written to disk or returned to the browser as a playable `.wav` file.

## Library Position

Every index has a unique position in a hierarchical "library":

| Level | Count |
| ------- | ------- |
| Walls per room | 4 |
| Shelves per wall | 5 |
| Albums per shelf | 32 |
| Tracks per album | 15 |
| **Items per room** | **9,600** |

The index integer is divided by 9,600. The quotient identifies the room (base64-encoded), and the remainder is decomposed into wall → shelf → album → track. This gives every index a browsable, human-friendly address and provides a perfect bijection between index values and library positions.

## Building the Project

### Prerequisites

- **CMake** 3.14+
- **MinGW-w64** (Windows) or **GCC** (Linux) with C++17
- **Boost** headers (Multiprecision) — fetched automatically on some setups, or install via your package manager
- **Emscripten SDK** (for WASM builds only)

### Native Build

**PowerShell (Windows):**

```powershell
.\tools\build.ps1 -Configuration Debug
```

**Bash (Linux / WSL):**

```bash
./tools/build.sh Debug
```

This creates the `build/` directory and compiles:

| Output | Description |
| -------- | ------------- |
| `build/audiolib.lib` | Static library |
| `build/tests_catch2.exe` | Unit tests |
| `build/example_main.exe` | Example CLI |
| `build/extract_index_cli.exe` | Extract an index from a WAV file |
| `build/reconstruct_cli.exe` | Reconstruct a WAV file from an index |

### Running Tests

```powershell
.\tools\run_tests.ps1          # PowerShell
./tools/run_tests.sh           # Bash
```

Tests use Catch2 v3 and cover base64 encoding/decoding, index generation/reconstruction, WAV parsing, metadata extraction, library position calculation, and end-to-end integration.

### Manual CMake Build

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
mingw32-make -j4
```

### Cleaning Build Artifacts

```powershell
.\tools\clean.ps1              # Clean build artifacts
.\tools\clean.ps1 -RemoveDir   # Delete the build/ directory entirely
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
| `docs/wasm/audio-index.js` | ES6 module loader |
| `docs/wasm/audio-index.wasm` | Compiled WebAssembly binary |

These files are committed to the repo so the GitHub Pages site works without a build step.

### WASM Build Configuration

- **Optimization:** `-O3` (Release) / `-O0 -g` (Debug)
- **Memory:** 64 MB initial, 2 GB max
- **Module format:** ES6 (`-sMODULARIZE`, `-sEXPORT_ES6`)
- **Bindings:** Emscripten embind (`--bind`)
- **Exceptions:** Enabled (`-fexceptions`)

## Web App

The `docs/` directory is a static site served by GitHub Pages. All audio processing runs in the browser via WASM — there is no backend.

| Page | Purpose |
| ------ | --------- |
| `index.html` | Home — overview and glossary |
| `browse.html` | Navigate the library by room / wall / shelf / album / track |
| `random.html` | Generate and play a random audio index |
| `search.html` | Search for audio by terms |
| `fileSearch.html` | Upload a WAV file and extract its index |

> **Note:** A "Record" page (record microphone audio and extract its index) previously
> existed but has been disabled and removed from site navigation. Its code is preserved
> in [`disabled-features/recording/`](disabled-features/recording/) for potential future use.

### Audio Constraints

| Parameter | Value |
| ----------- | ------- |
| Format | WAV (PCM) |
| Sample rate | 44,100 Hz |
| Bit depth | 16-bit |
| Channels | Mono |
| Max duration | 2 minutes |

## License

See repository for license details.
