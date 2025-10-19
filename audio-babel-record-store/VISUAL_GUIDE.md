# Visual Guide: Audio Index to Library Position

## Overview

This diagram shows how an audio file becomes an index and gets placed in the library structure.

## Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                         AUDIO FILE                                  │
│                    (Max 2 minutes, WAV)                             │
└───────────────────────────────┬─────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      C++ AUDIO INDEX LIBRARY                        │
│                  (cpp/src/AudioIndex.cpp)                           │
│                                                                     │
│  1. Extract PCM samples                                             │
│  2. Pack into big integer (boost::multiprecision::cpp_int)          │
│  3. Encode as URL-safe base64 (no padding)                          │
└───────────────────────────────┬─────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      BASE64 INDEX STRING                            │
│              "dGVzdC1hdWRpby1pbmRleC1leGFtcGxl"                      │
│                   (Unique identifier)                               │
└───────────────────────────────┬─────────────────────────────────────┘
                                │
                    ┌───────────┴───────────┐
                    ▼                       ▼
    ┌───────────────────────┐   ┌─────────────────────────┐
    │  METADATA EXTRACTION  │   │  POSITION CALCULATION   │
    │  (indexParser.js)     │   │  (positionEncoder.js)   │
    │                       │   │                         │
    │  Split into segments: │   │  Hash index to get:     │
    │  • Genre (g5Kj2)      │   │  • Room ID              │
    │  • Artist (a9Xm3)     │   │  • Wall (1-4)           │
    │  • Album (al7Qn1)     │   │  • Shelf (1-20)         │
    │  • Track (t4Rp6)      │   │  • Track (1-32)         │
    └───────────┬───────────┘   └─────────┬───────────────┘
                │                         │
                └────────────┬────────────┘
                             ▼
         ┌───────────────────────────────────────┐
         │         COMBINED METADATA             │
         │                                       │
         │  Index: "dGVzdC1hdWRpby1pbmRleC..."  │
         │  Position:                            │
         │    - roomId: "room_abc12345"          │
         │    - wall: 1                          │
         │    - shelf: 5                         │
         │    - track: 17                        │
         │    - address: "room_abc12345:W1:S05:T17" │
         │  Metadata:                            │
         │    - genre: "g5Kj2"                   │
         │    - artist: "a9Xm3"                  │
         │    - album: "al7Qn1"                  │
         │    - track: "t4Rp6"                   │
         └────────────────┬──────────────────────┘
                          ▼
         ┌────────────────────────────────────────┐
         │         CREATE TRACK OBJECT            │
         │         (models/Track.js)              │
         └────────────────┬───────────────────────┘
                          ▼
         ┌────────────────────────────────────────┐
         │      PLACE IN LIBRARY STRUCTURE        │
         │                                        │
         │  Room (genre_label)                    │
         │    └─ Wall 1 (artist_label)            │
         │         └─ Shelf 5 (album_label)       │
         │              └─ Track 17 (track_label) │
         └────────────────────────────────────────┘
```

## Library Structure Detail

```
Room "room_abc12345" (Genre: "g5Kj2")
├─ Wall 1 (Artist: "a9Xm3")
│  ├─ Shelf 1 (Album: "al...")  [32 tracks]
│  ├─ Shelf 2 (Album: "...")    [32 tracks]
│  ├─ Shelf 3 (Album: "...")    [32 tracks]
│  ├─ ...
│  ├─ Shelf 5 (Album: "al7Qn1") [32 tracks]
│  │  ├─ Track 1  [empty]
│  │  ├─ Track 2  [empty]
│  │  ├─ ...
│  │  ├─ Track 17 [★ OUR TRACK ★]
│  │  ├─ Track 18 [empty]
│  │  ├─ ...
│  │  └─ Track 32 [empty]
│  ├─ ...
│  └─ Shelf 20 (Album: "...")   [32 tracks]
├─ Wall 2 (Artist: "...")
│  └─ [20 shelves × 32 tracks each]
├─ Wall 3 (Artist: "...")
│  └─ [20 shelves × 32 tracks each]
└─ Wall 4 (Artist: "...")
   └─ [20 shelves × 32 tracks each]

Total Capacity: 4 walls × 20 shelves × 32 tracks = 2,560 tracks per room
```

## Position Calculation Detail

```
Input: Base64 Index = "dGVzdC1hdWRpby1pbmRleC1leGFtcGxl"

Step 1: Calculate Room ID
  roomHash = hash(first 16 chars of index)
  roomId = "room_" + hex(roomHash)
  → "room_abc12345"

Step 2: Calculate Linear Position
  fullHash = hash(entire index)
  linearPosition = fullHash % 2560  (0-2559)
  → e.g., 144

Step 3: Decode to Hierarchical Position
  wall  = floor(144 / 640) + 1  = 1
  remainder = 144 % 640 = 144
  shelf = floor(144 / 32) + 1   = 5
  track = (144 % 32) + 1        = 17

Result: Wall 1, Shelf 5, Track 17
Address: "room_abc12345:W1:S05:T17"
```

## Metadata Extraction Detail

```
Input: Base64 Index = "dGVzdC1hdWRpby1pbmRleC1leGFtcGxl"
                      (32 characters)

Step 1: Calculate Weights
  For each character, add ASCII value to weight[i % 4]
  weights = [w0, w1, w2, w3]

Step 2: Proportional Lengths
  totalWeight = w0 + w1 + w2 + w3
  len[0] = floor((32 × w0) / totalWeight)
  len[1] = floor((32 × w1) / totalWeight)
  len[2] = floor((32 × w2) / totalWeight)
  len[3] = floor((32 × w3) / totalWeight)

Step 3: Adjust to Total Length
  Distribute remaining characters
  → e.g., [8, 8, 8, 8]

Step 4: Extract Segments
  genre  = index[0:8]    = "dGVzdC1h"
  artist = index[8:16]   = "dWRpby1p"
  album  = index[16:24]  = "bmRleC1l"
  track  = index[24:32]  = "eGFtcGxl"

These serve as display labels, not actual metadata!
```

## Navigation Example

```javascript
// Given address: "room_abc12345:W1:S05:T17"

// Method 1: Direct navigation
const track = room.getWall(1).getShelf(5).getTrack(17);

// Method 2: Parse address
const pos = parseAddressString("room_abc12345:W1:S05:T17");
const track = room.getWall(pos.wall)
                  .getShelf(pos.shelf)
                  .getTrack(pos.track);

// Method 3: From index
const parsed = parseIndex(base64Index);
const track = room.getWall(parsed.position.wall)
                  .getShelf(parsed.position.shelf)
                  .getTrack(parsed.position.track);
```

## Size Constraints Visualization

```
Audio Duration vs Index Size:

30s  audio → ~2.6 MB  → ~3.5 MB base64
60s  audio → ~5.3 MB  → ~7.1 MB base64
90s  audio → ~7.9 MB  → ~10.6 MB base64
120s audio → ~10.6 MB → ~14.1 MB base64 ← MAX

Audio Format:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Sample Rate:  44,100 Hz (CD quality)
Bit Depth:    16-bit
Channels:     1 (mono)
Max Duration: 120 seconds
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

## Key Principles

1. **Deterministic**: Same audio → same index → same position
2. **Unique**: Content-based hashing ensures uniqueness
3. **Scalable**: Unlimited rooms, each holds 2,560 tracks
4. **Hierarchical**: Clear navigation path
5. **Size-limited**: 2-minute max keeps indexes manageable
6. **Compatible**: Works with existing C++ library

---

See `INDEX_ARCHITECTURE.md` for implementation details.
See `IMPLEMENTATION_SUMMARY.md` for code overview.
See `examples/usage-example.js` for working code.
