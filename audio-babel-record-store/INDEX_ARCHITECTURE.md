# Audio Babel Record Store - Index Architecture

## Overview

This document describes how audio indexes are generated, encoded, and mapped to the Library of Babel record store structure.

## Hierarchical Structure

The record store follows the Library of Babel's architecture:

```
Room (Hexagon/Genre)
  └─ Wall (Artist) × 4
      └─ Shelf (Longbox/Album) × 20
          └─ Track (Book) × 32
```

**Capacity per room**: 4 walls × 20 shelves × 32 tracks = **2,560 tracks**

## Audio Constraints

To ensure manageable index sizes and consistent behavior:

- **Maximum Duration**: 2 minutes (120 seconds)
- **Sample Rate**: 44,100 Hz (CD quality)
- **Bit Depth**: 16-bit
- **Channels**: 1 (mono)
- **Maximum Samples**: 5,292,000
- **Maximum Sample Data**: ~10.6 MB
- **Format**: WAV (PCM)

### Size Calculations

For a 2-minute audio file:
- **Samples**: 44,100 samples/sec × 120 sec = 5,292,000 samples
- **Bytes**: 5,292,000 samples × 2 bytes/sample = 10,584,000 bytes (~10.1 MB)
- **With Header**: 10,584,000 + 16 = 10,584,016 bytes
- **Base64 Encoded**: ~14,112,021 characters

## Index Generation Process

### 1. Audio to Index (C++ Library)

The C++ library (`cpp/src/AudioIndex.cpp`) converts audio data to a big integer index:

```
Index Structure:
┌─────────────────────────────────────┬──────────────────┐
│     PCM Samples (MSB-first)         │   16-byte Header │
└─────────────────────────────────────┴──────────────────┘
```

**Header Format** (16 bytes, big-endian):
- Bytes 0-3: Sample rate (uint32)
- Bytes 4-5: Bit depth (uint16)
- Bytes 6-7: Number of channels (uint16)
- Bytes 8-15: Number of frames (uint64)

The index is then encoded as **URL-safe base64** without padding:
- Alphabet: `A-Z a-z 0-9 - _`
- No `=` padding characters

### 2. Position Encoding

Each index is deterministically mapped to a position in the library:

```javascript
// Calculate room from index hash
roomId = hash(index_prefix) → "room_abc12345"

// Calculate position from full index hash
linearPosition = hash(full_index) % 2560  // 0-2559

// Decode to hierarchical position
wall  = floor(linearPosition / 640) + 1     // 1-4
shelf = floor((linearPosition % 640) / 32) + 1  // 1-20
track = (linearPosition % 32) + 1            // 1-32
```

### 3. Metadata Extraction

Metadata segments are derived from the base64 index string itself (matching C++ implementation):

```javascript
// Split index into 4 weighted segments
weights = [sum of chars at position i%4 for i in 0..length]
genre   = index[0:len1]
artist  = index[len1:len1+len2]
album   = index[len1+len2:len1+len2+len3]
track   = index[len1+len2+len3:end]
```

These segments serve as **display labels** (not actual metadata).

## Address Format

Each track has a unique address:

```
room_abc12345:W1:S05:T17
├─────────────┘│  │   └─ Track number (1-32)
│              │  └───── Shelf number (1-20)
│              └──────── Wall number (1-4)
└─────────────────────── Room ID (hash-based)
```

## JavaScript Services

### `positionEncoder.js`

Handles position calculations:

- `getPositionFromIndex(base64Index)` - Get full position metadata
- `calculateRoomId(base64Index)` - Calculate room identifier
- `calculateLinearPosition(base64Index)` - Get position within room (0-2559)
- `decodePosition(base64Index, linearPosition)` - Convert linear to hierarchical
- `formatAddress(roomId, wall, shelf, track)` - Create address string

### `indexParser.js`

Parses indexes and extracts metadata:

- `parseIndex(base64Index)` - Parse complete index with position and metadata
- `extractMetadataFromIndex(base64Index)` - Get display labels
- `isValidBase64Url(str)` - Validate format
- `getIndexPreview(base64Index, maxLength)` - Get shortened preview

### `audioConstraints.js`

Defines and validates audio constraints:

- `isValidDuration(seconds)` - Check if duration is within limits
- `calculateIndexSize(duration)` - Estimate index size
- `getRecommendedSettings()` - Get audio format recommendations

## Data Models

### Room

Represents a genre/hexagon:

```javascript
Room {
  roomId: "room_abc12345",
  genreLabel: "g5Kj2",
  walls: [Wall, Wall, Wall, Wall]  // Always 4 walls
}
```

### Wall

Represents an artist:

```javascript
Wall {
  wallNumber: 1,
  artistLabel: "a9Xm3",
  shelves: [Shelf × 20]  // Always 20 shelves
}
```

### Shelf

Represents an album/longbox:

```javascript
Shelf {
  shelfNumber: 5,
  albumLabel: "al7Qn1",
  tracks: [Track × 32]  // Always 32 tracks
}
```

### Track

Represents an individual track/book:

```javascript
Track {
  index: "dGVzdC1hdWRpby1pbmRleA",  // Base64 index
  position: {
    roomId: "room_abc12345",
    wall: 1,
    shelf: 5,
    track: 17,
    linearPosition: 144,
    address: "room_abc12345:W1:S05:T17"
  },
  metadata: {
    genre: "g5Kj2",
    artist: "a9Xm3",
    album: "al7Qn1",
    track: "t4Rp6"
  },
  duration: 95.5,
  coverSvg: "<svg>...</svg>"
}
```

## Usage Examples

### Creating a Track from Audio

```javascript
import { parseIndex } from './services/indexParser.js';
import Track from './models/Track.js';

// After C++ generates the base64 index from audio
const base64Index = "dGVzdC1hdWRpby1pbmRleC1leGFtcGxl";

// Parse the index
const parsed = parseIndex(base64Index);

// Create track
const track = new Track(
  parsed.index,
  parsed.position,
  parsed.metadata,
  95.5,  // duration in seconds
  null   // coverSvg (optional)
);

console.log(track.address);
// Output: "room_abc12345:W1:S05:T17"

console.log(track.getFullPath());
// Output: "room_abc12345 / Wall 1 / Shelf 5 / Track 17"
```

### Navigating the Library

```javascript
import Room from './models/Room.js';
import Wall from './models/Wall.js';
import Shelf from './models/Shelf.js';

// Create a room
const room = new Room("room_abc12345", "g5Kj2");

// Create a wall
const wall = new Wall(1, "a9Xm3");

// Create a shelf
const shelf = new Shelf(5, "al7Qn1");

// Add track to shelf
shelf.setTrack(17, track);

// Add shelf to wall
wall.setShelf(5, shelf);

// Add wall to room
room.setWall(1, wall);

// Navigate to track
const foundTrack = room.getWall(1).getShelf(5).getTrack(17);
console.log(foundTrack.address);
// Output: "room_abc12345:W1:S05:T17"
```

## Uniqueness Guarantees

1. **Content-based**: The index is derived from the audio content itself, so identical audio produces identical indexes
2. **Cryptographic**: The base64 encoding preserves the uniqueness of the underlying big integer
3. **Deterministic positioning**: Hash functions ensure the same index always maps to the same position
4. **Collision resistance**: With 2,560 slots per room and unlimited rooms, the probability of collision is negligible

## Limitations

1. **Fixed capacity per room**: Each room can hold exactly 2,560 tracks
2. **No reordering**: Positions are deterministically calculated and cannot be manually reassigned
3. **Duration limit**: Audio longer than 2 minutes must be split or truncated
4. **Format restriction**: Only WAV PCM format is supported by the C++ library

## Future Enhancements

1. **Room overflow handling**: When a room is full, create a new room with the same genre label
2. **Search indexing**: Build search trees for efficient navigation
3. **Compression**: Implement optional compression for indexes > 1MB
4. **Multi-format support**: Extend C++ library to support MP3, FLAC, etc.

---

For implementation details, see:
- C++ implementation: `cpp/src/AudioIndex.cpp`
- Position encoding: `audio-babel-record-store/src/services/positionEncoder.js`
- Index parsing: `audio-babel-record-store/src/services/indexParser.js`
