# Audio Index Header System

## Overview

The audio indexing system now uses a **13-byte header** to preserve complete audio metadata within the index itself. This solves the critical bug where different durations of silence (or audio with leading zeros) would produce identical indexes.

## Architecture: Frontend vs Backend

### Backend (C++ Library)
- **Always uses full indexes with 13-byte header**
- Stores: `[HEADER (13 bytes)][PCM data]`
- Header contains: version, num_frames, sample_rate, bit_depth, num_channels
- This is the "ground truth" format used internally

### Frontend (User-Facing)
- **Users see/input PCM data only** (no header)
- Cleaner, shorter indexes for sharing
- Header is added/stripped transparently by JavaScript layer

### Interface Layer
JavaScript functions bridge the gap:
```javascript
// User inputs: "ABCD1234" (base64 PCM only)
// Backend needs: "[13-byte-header]ABCD1234"

const fullIndex = addIndexHeader(userIndex, audioParams);
const userIndex = stripIndexHeader(fullIndex);
```

---

## Header Format (13 bytes, little-endian)

```
Byte 0:      VERSION (0x01)
Byte 1-4:    num_frames (uint32_t)   - Number of audio frames
Byte 5-8:    sample_rate (uint32_t)  - Sample rate in Hz (e.g., 44100)
Byte 9-10:   bit_depth (uint16_t)    - Bits per sample (8, 16, or 32)
Byte 11-12:  num_channels (uint16_t) - Number of channels (1=mono, 2=stereo)
Byte 13+:    PCM sample data
```

### Example
For 1 second of 44.1kHz, 16-bit, mono audio:
- num_frames = 44100
- sample_rate = 44100
- bit_depth = 16
- num_channels = 1
- PCM data = 88200 bytes (44100 frames × 2 bytes)
- Total index size = 13 + 88200 = 88213 bytes

---

## JavaScript API

Located in `docs/js/utils.js`

### Core Functions

#### `encodeBase64Url(bytes: Uint8Array): string`
Encode bytes to URL-safe base64 (no padding).
- Alphabet: `A-Z a-z 0-9 - _`
- No `=` padding (matches C++ implementation)

#### `decodeBase64Url(base64Url: string): Uint8Array`
Decode URL-safe base64 to bytes.
- Throws error on invalid characters
- Handles variable-length input

#### `addIndexHeader(userIndexBase64, audioParams): string`
Add 13-byte header to PCM-only index.

**Parameters:**
```javascript
audioParams = {
  numFrames: number,     // Required
  sampleRate: number,    // Default: 44100
  bitDepth: number,      // Default: 16
  numChannels: number    // Default: 1
}
```

**Example:**
```javascript
import { addIndexHeader } from './js/utils.js';

const pcmOnlyIndex = "ABCD1234EF"; // User's index
const fullIndex = addIndexHeader(pcmOnlyIndex, {
  numFrames: 5292,
  sampleRate: 44100,
  bitDepth: 16,
  numChannels: 1
});
// fullIndex now has 13-byte header prepended
```

#### `stripIndexHeader(fullIndexBase64): string`
Strip 13-byte header from full index.

**Returns:** PCM-only index (user-facing format)

**Example:**
```javascript
import { stripIndexHeader } from './js/utils.js';

const fullIndex = "..."; // From WASM or internal storage
const userIndex = stripIndexHeader(fullIndex);
// Display userIndex to user (cleaner, shorter)
```

#### `parseIndexHeader(fullIndexBase64): Object|null`
Extract metadata from header without decoding entire index.

**Returns:**
```javascript
{
  version: 1,
  numFrames: 44100,
  sampleRate: 44100,
  bitDepth: 16,
  numChannels: 1,
  duration: 1.0,        // Calculated: numFrames / sampleRate
  pcmByteCount: 88200   // Size of PCM data in bytes
}
```

Returns `null` if no valid header found.

---

## Typical Workflows

### Recording Audio → Generating Index

```javascript
// 1. User records audio
const audioBuffer = recordAudio();

// 2. Extract PCM samples
const pcmSamples = extractPCM(audioBuffer);
const pcmBase64 = encodeBase64Url(pcmSamples);

// 3. Calculate num_frames
const numFrames = audioBuffer.length;

// 4. Create full index with header (for WASM processing)
const fullIndex = addIndexHeader(pcmBase64, {
  numFrames: numFrames,
  sampleRate: 44100,
  bitDepth: 16,
  numChannels: 1
});

// 5. Pass to WASM to generate metadata
const metadata = wasmModule.indexToMetadata(fullIndex);

// 6. Strip header for user display
const userIndex = stripIndexHeader(fullIndex);

// 7. Show clean index to user
displayIndex(userIndex); // e.g., "Ox3jK9..."
```

### User Inputs Index → Playing Audio

```javascript
// 1. User inputs their index (PCM only)
const userInput = getUserInput(); // e.g., "Ox3jK9..."

// 2. User tells us duration (or we infer from metadata)
const duration = 2.0; // seconds
const numFrames = Math.floor(duration * 44100);

// 3. Add header for WASM processing
const fullIndex = addIndexHeader(userInput, {
  numFrames: numFrames,
  sampleRate: 44100,
  bitDepth: 16,
  numChannels: 1
});

// 4. Reconstruct audio via WASM
const audioData = wasmModule.indexToAudioData(fullIndex);

// 5. Play audio
playAudio(audioData);
```

### Inspecting Index Metadata

```javascript
// Quick peek at index info without full decoding
const fullIndex = "...";
const info = parseIndexHeader(fullIndex);

if (info) {
  console.log(`Duration: ${info.duration.toFixed(2)}s`);
  console.log(`Sample Rate: ${info.sampleRate} Hz`);
  console.log(`Bit Depth: ${info.bitDepth}-bit`);
}
```

---

## Testing

### Test Page
Open `docs/test-header-utils.html` in a browser to run comprehensive tests:

1. **Base64 URL Encoding/Decoding** - Verify codec works correctly
2. **Add Header** - Check header structure and values
3. **Strip Header** - Verify header removal
4. **Parse Header** - Test metadata extraction
5. **Round-trip** - Add + Strip should return original PCM
6. **Silence Duration** - THE BUG FIX - different silence durations produce different indexes

### Expected Results
All tests should pass. Specifically:
- ✓ 1 second and 2 seconds of silence have **different** indexes
- ✓ Duration is correctly preserved in header
- ✓ Round-trip (add + strip) returns original data

---

## C++ Implementation

Located in `cpp/src/AudioIndex.cpp`

### `audioDataToIndex()`
- Builds 13-byte header with audio parameters
- Appends PCM sample data
- Converts to `cpp_int` (big integer)

### `indexToAudioData()`
- Validates version byte (must be 0x01)
- Reads header fields (little-endian)
- Validates header consistency with PCM data size
- Extracts PCM samples
- Returns `AudioData` struct with all parameters

### Error Handling
- Throws `std::runtime_error` for:
  - Invalid version byte
  - Unsupported bit depth
  - Header-PCM size mismatch
  - Index too small (< 13 bytes)

---

## Migration & Compatibility

### Version Byte
- `0x01` = Current format (13-byte header)
- Future versions can use `0x02`, `0x03`, etc.

### Backward Compatibility
Old indexes (without header) will fail validation with clear error message. Users must re-record or regenerate indexes.

### Forward Compatibility
JavaScript layer can detect version byte and handle different header formats in the future.

---

## Benefits

### ✅ Solves Silence Bug
Different durations of silence now produce unique indexes:
```
1 second silence: [0x01][44100 frames][...] → Index A
2 seconds silence: [0x01][88200 frames][...] → Index B
A ≠ B ✓
```

### ✅ Preserves Leading Zeros
Audio starting with silence/zeros no longer loses duration info.

### ✅ Exact Reconstruction
Can reconstruct audio with correct:
- Duration
- Sample rate
- Bit depth
- Channel count

### ✅ Self-Contained
Index contains everything needed - no external metadata required.

### ✅ Clean User Experience
Users see short PCM-only indexes, complexity is hidden.

---

## Constants

### Web App Defaults
- Sample Rate: **44100 Hz**
- Bit Depth: **16-bit**
- Channels: **1 (mono)**
- Max Duration: **120 seconds (2 minutes)**

### Header Size
- **13 bytes** (0.01% overhead for 2-minute audio)

### Base64 Alphabet
- **URL-safe**: `A-Z a-z 0-9 - _`
- **No padding** (no `=` characters)

---

## Troubleshooting

### "Invalid index: unsupported version byte"
- Index was created without header (old format)
- Solution: Re-record or regenerate the index

### "Index header mismatch"
- Header's num_frames doesn't match PCM data size
- Corruption or tampering detected
- Solution: Regenerate index from source audio

### "numFrames is required in audioParams"
- Must provide `numFrames` when calling `addIndexHeader()`
- Calculate from audio duration: `numFrames = duration * sampleRate`

---

## Future Enhancements

### Possible Future Features
1. **Compression flag** - Indicate if PCM data is compressed
2. **Checksum** - Detect data corruption
3. **Extended metadata** - Store title, artist, etc. in header
4. **Variable sample rates** - Support non-44.1kHz audio
5. **Multi-channel** - Support stereo or surround sound

### Header Version 2 (Hypothetical)
```
Byte 0:      VERSION (0x02)
Byte 1-4:    num_frames (uint32_t)
Byte 5-8:    sample_rate (uint32_t)
Byte 9-10:   bit_depth (uint16_t)
Byte 11-12:  num_channels (uint16_t)
Byte 13-16:  checksum (uint32_t) [NEW]
Byte 17:     flags (compression, etc.) [NEW]
Byte 18+:    PCM data
```

---

## References

- **C++ Implementation**: `cpp/src/AudioIndex.cpp`
- **JavaScript API**: `docs/js/utils.js`
- **Test Suite**: `cpp/tests/test_main.cpp` (test: "silence duration preservation")
- **Browser Tests**: `docs/test-header-utils.html`
- **WASM Wrapper**: `docs/js/audioIndexWasm.js`

---

## Contact & Support

For questions or issues:
1. Check test page (`test-header-utils.html`)
2. Review C++ unit tests (`cpp/tests/test_main.cpp`)
3. Verify WASM build is up-to-date
4. Check browser console for JavaScript errors

---

**Last Updated**: October 27, 2025  
**Version**: 1.0  
**Status**: ✅ Implemented and tested
