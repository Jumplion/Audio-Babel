# Position Index System with Audio Headers

## Overview

The Speaker of Babel uses a hierarchical position system to organize audio files in an infinite "Record Shop" structure. With the addition of the 13-byte audio header system, the relationship between positions and audio indexes has been clarified and properly implemented.

## Hierarchy Structure

```
Room (∞) → Wall (4) → Shelf (5) → Album (32) → Track (15)
```

- **Rooms**: Infinite (0, 1, 2, ...)
- **Walls per Room**: 4 (representing "genre")
- **Shelves per Wall**: 5 (representing "artist")
- **Albums per Shelf**: 32
- **Tracks per Album**: 15

### Calculated Constants
- Tracks per Album: 15
- Items per Shelf: 480 (32 × 15)
- Items per Wall: 2,400 (5 × 480)
- Items per Room: 9,600 (4 × 2,400)

## How Positions Work with Headers

### Audio to Position Flow
1. **Audio File** → 13-byte header + PCM data
2. **audioDataToIndex()** → cpp_int (big integer representing [header|PCM])
3. **calculateLibraryPosition(cpp_int)** → Position (Room, Wall, Shelf, Album, Track)
   - Uses modular arithmetic to map the full index to position
   - Room = index / 9600
   - Wall = (index % 9600) / 2400
   - Shelf = (index % 2400) / 480
   - Album = (index % 480) / 15
   - Track = index % 15

### Position to Audio Flow (Browsing)
1. **Position** (Room, Wall, Shelf, Album, Track)
2. **reconstructIndexFromPosition()** → Mathematical index (Room×9600 + Wall×2400 + Shelf×480 + Album×15 + Track)
3. **Generate Synthetic Audio**:
   - Convert mathematical index to bytes (PCM data)
   - Pad/truncate to reasonable length (~1-2 seconds)
   - **Prepend 13-byte header** with default parameters:
     - VERSION: 0x01
     - num_frames: calculated from PCM size
     - sample_rate: 44100 Hz
     - bit_depth: 16 bits
     - num_channels: 1 (mono)
4. **Encode to base64** → Valid audio index

## Critical Implementation Details

### The Header Problem (Solved)
**Problem**: Position calculations use raw mathematical indexes, but audio reconstruction requires a valid 13-byte header structure.

**Solution**: The WASM function `reconstructIndexFromPosition()` generates synthetic audio:
- Takes position coordinates as input
- Calculates mathematical index
- Treats that index as PCM audio data
- **Adds proper header** before encoding to base64
- Returns a valid, playable audio index

### Why This Matters
Without the header injection:
- Position → Index would return pure numbers
- `indexToAudioData()` would fail (expects header at bytes 0-12)
- Browse feature would be broken

With header injection:
- Every position maps to a valid, playable audio file
- Different positions produce different audio (header + unique PCM)
- System maintains bidirectional mapping: Audio ↔ Position

## Code Locations

### C++ Implementation
- **Position Calculation**: `cpp/src/LibraryPosition.cpp`
  - `calculateLibraryPosition()`: Index → Position
  - `reconstructIndexFromPosition()`: Position → Mathematical Index
  
- **Audio System**: `cpp/src/AudioIndex.cpp`
  - `audioDataToIndex()`: Adds header, converts to cpp_int
  - `indexToAudioData()`: Validates header, extracts audio

- **WASM Bindings**: `cpp/wasm/wasm_bindings.cpp`
  - `reconstructIndexFromPosition()`: **Generates synthetic audio WITH header**
  - Exposed to JavaScript as `module.reconstructIndex()`

### JavaScript Implementation
- **Position Encoder**: `docs/js/positionEncoder.js`
  - Pure JS implementation of position calculations
  - Used for client-side position display

- **Browse Page**: `docs/js/browse.js`
  - Uses WASM `reconstructIndex()` to generate audio from positions
  - Displays hierarchical navigation UI

- **WASM Wrapper**: `docs/js/audioIndexWasm.js`
  - Provides clean API for WASM module
  - `reconstructAudioFromIndex()`: Decodes audio from base64

## Testing

### C++ Tests
The test suite includes comprehensive position verification:
- **Position Calculation**: Small index, boundaries, large numbers
- **Roundtrip**: Position → Index → Position (bijection verification)
- **With Headers**: Verifies position calculations work with full audio indexes (header + PCM)
- **Uniqueness**: Different audio durations/params map to different positions

Run tests: `.\tools\run_tests.ps1` or `.\build\tests_runner.exe`

### Integration Flow Test
A complete flow test (added in test_main.cpp):
1. Create audio with different durations (1 sec vs 2 sec)
2. Convert to indexes (includes headers)
3. Calculate positions from indexes
4. Verify positions are different
5. Roundtrip: Position → Index → Position
6. Reconstruct audio from position-derived index
7. Verify audio parameters preserved (sample rate, bit depth, frames)

## Browse Feature Implementation

The `browse.html` page demonstrates the position system:

```javascript
// User selects: Room 5, Wall 2, Shelf 1, Album 3, Track 7
const base64Index = wasm.module.reconstructIndex(
    "5",    // room (as string for BigInt support)
    2,      // wall
    1,      // shelf
    3,      // album
    7       // track
);

// Returns: Valid base64 audio index WITH header
// Can be played, analyzed, downloaded like any audio file
```

### What Happens Internally
1. WASM calculates: 5×9600 + 2×2400 + 1×480 + 3×15 + 7 = 52,777
2. Converts 52,777 to PCM bytes
3. Pads to ~1 second (88,200 bytes for 16-bit mono at 44.1kHz)
4. Builds header:
   ```
   [0x01][num_frames][44100][16][1][...PCM data...]
   ```
5. Encodes to base64 (URL-safe, no padding)
6. Returns to JavaScript for playback

## Header Format Reference

All audio indexes include a 13-byte header (little-endian):

| Offset | Size | Field        | Description              |
|--------|------|--------------|--------------------------|
| 0      | 1    | VERSION      | Always 0x01              |
| 1-4    | 4    | num_frames   | Frame count (uint32)     |
| 5-8    | 4    | sample_rate  | Hz (uint32)              |
| 9-10   | 2    | bit_depth    | Bits (uint16)            |
| 11-12  | 2    | num_channels | Channel count (uint16)   |
| 13+    | var  | PCM data     | Audio samples            |

## Migration Notes

**Before Header System**:
- Positions calculated from raw PCM data only
- Silence of any duration → same index (bug!)
- Browse feature theoretical but unimplemented

**After Header System**:
- Positions calculated from [header + PCM]
- Different durations → different indexes ✓
- Browse feature fully functional ✓
- Every position → valid audio file ✓

## Future Enhancements

Potential improvements:
1. **Deterministic Audio Generation**: Use position as seed for procedural audio synthesis
2. **Cover Art from Position**: Generate unique SVG covers based on position coordinates
3. **Position-based Search**: Find audio files by navigating the hierarchy
4. **Room Themes**: Map Wall numbers to actual musical genres
5. **Compression**: Explore more compact position encoding schemes

## API Reference

### C++ Functions
```cpp
// Calculate position from audio index
LibraryPosition calculateLibraryPosition(const cpp_int& index);

// Reconstruct mathematical index from position
cpp_int reconstructIndexFromPosition(const LibraryPosition& pos);
```

### WASM Functions (JavaScript)
```javascript
// Generate valid audio index from position (WITH header)
module.reconstructIndex(roomStr, wall, shelf, album, track) → base64String

// Get metadata from any base64 index
module.getMetadata(base64Index) → jsonString

// Decode audio from base64 index
module.reconstructAudio(base64Index) → Uint8Array
```

## Related Documentation
- [Header System Documentation](./HEADER_SYSTEM.md)
- [Browse Hierarchical Navigation](./BROWSE_HIERARCHICAL_NAV.md)
- [Architecture Overview](../README.md)
