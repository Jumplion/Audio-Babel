# Hierarchical Browse Navigation - Implementation Guide

## Overview

The Browse page has been completely redesigned to provide a hierarchical navigation experience inspired by the Library of Babel. Users navigate through a deterministic structure: **Room → Wall → Shelf → Album → Track**.

## Navigation Hierarchy

```
Room (user input: any integer)
  └── 4 Walls (genres)
      └── 5 Shelves per wall
          └── 32 Albums per shelf
              └── 15 Tracks per album
                  
Total: 9,600 unique audio tracks per room
```

## Architecture

### C++ Backend (Position Calculation)

**Files:**
- `cpp/include/LibraryPosition.h` - Position structure and calculation functions
- `cpp/src/LibraryPosition.cpp` - Implementation with modular arithmetic
- `cpp/include/IndexMetadata.h` - Extended with `position` field
- `cpp/src/IndexMetadata.cpp` - Both `extractMetadataFromIndex` overloads calculate position

**Key Functions:**
```cpp
LibraryPosition calculateLibraryPosition(const cpp_int& index)
cpp_int reconstructIndexFromPosition(const LibraryPosition& pos)
```

**Algorithm:**
- Forward: Divide index by hierarchy constants using modular arithmetic
- Reverse: Multiply and sum to reconstruct index from position
- Deterministic bijection ensures no collisions

### JavaScript Frontend (Navigation UI)

**Files:**
- `docs/browse.html` - Restructured with progressive disclosure sections
- `docs/js/browse.js` - Hierarchical navigation logic (uses WASM for position calculations)
- `docs/css/browse.css` - Styles for hierarchical UI elements
- `cpp/wasm/wasm_bindings.cpp` - WASM bindings exposing position calculation functions

**Key Components:**

1. **Room Input** (`#roomSection`)
   - User enters any integer (room number)
   - Triggers wall selection display

2. **Wall Selection** (`#wallSection`)
   - SVG hexagon with 4 clickable polygons
   - Each wall represents a genre
   - Hover effects and click interactions

3. **Shelf Selection** (`#shelfSection`)
   - Grid of 5 shelf buttons (0-4)
   - Displays after wall selection

4. **Album Selection** (`#albumSection`)
   - Grid of 32 album buttons (0-31)
   - Displays after shelf selection

5. **Track Selection** (`#trackSection`)
   - Grid of 15 track buttons (0-14)
   - Displays after album selection

6. **Result Display** (`#resultSection`)
   - Generates audio from calculated index
   - Displays metadata (genre, artist, album, track)
   - Shows SVG cover art
   - Plays audio

## Navigation State

```javascript
const navState = {
    room: null,     // BigInt - user input
    wall: null,     // 0-3
    shelf: null,    // 0-4
    album: null,    // 0-31
    track: null     // 0-14
};
```

## Flow Diagram

```
User enters Room number
    ↓
enterRoom() validates & stores room
    ↓
showSection('wallSection') displays hexagon
    ↓
User clicks a wall polygon
    ↓
selectWall(wallNum) stores wall & renders shelves
    ↓
showSection('shelfSection') displays 5 buttons
    ↓
User clicks shelf button
    ↓
selectShelf(shelfNum) stores shelf & renders albums
    ↓
showSection('albumSection') displays 32 buttons
    ↓
User clicks album button
    ↓
selectAlbum(albumNum) stores album & renders tracks
    ↓
showSection('trackSection') displays 15 buttons
    ↓
User clicks track button
    ↓
selectTrack(trackNum) stores track
    ↓
reconstructIndexFromPosition() calculates audio index
    ↓
generateAndDisplayTrack() creates audio & metadata
    ↓
showSection('resultSection') displays result
```

## Key Functions

### browse.js

```javascript
// Navigation state management
const navState = {
    room: null,     // BigInt - user input
    wall: null,     // 0-3
    shelf: null,    // 0-4
    album: null,    // 0-31
    track: null     // 0-14
};

// WASM position functions (exposed from C++)
// wasm.module.reconstructIndex(roomStr, wall, shelf, album, track) -> base64 string
// wasm.module.calculatePosition(base64Index) -> JSON {room, wall, shelf, album, track}
```

## CSS Styling

### Key Classes

- `.browse-intro` - Introduction text styling
- `.nav-section` - Container for each navigation level
- `.input-row` - Room input layout
- `.hexagon-container` - Centers SVG hexagon
- `.hexagon-svg` - 300×300px SVG canvas
- `.wall` - Polygon fill, stroke, hover effects
- `.wall-label` - White text labels on walls
- `.shelves-container` - CSS Grid (150px min columns)
- `.shelf-btn` - Blue theme buttons (hover lift effect)
- `.albums-container` - CSS Grid (120px min columns)
- `.album-btn` - Purple theme buttons
- `.tracks-container` - CSS Grid (140px min columns)
- `.track-btn` - Brown theme buttons
- `#breadcrumb` - Navigation path display
- `#resultContainer` - Final audio result display

### Responsive Design

```css
@media (max-width: 768px) {
    - Hexagon shrinks to 250×250px
    - Grid columns reduce to 100px minimum
    - Input row stacks vertically
}
```

## Testing Status

### C++ Backend
✅ All 53 tests passing
- 45 existing IndexMetadata tests
- 8 new LibraryPosition tests
  - Small index calculation
  - Roundtrip index ↔ position
  - Room boundary behavior
  - Large index handling
  - Metadata integration
  - cpp_int overload
  - Zero index edge case
  - Constants validation

### JavaScript Frontend
⚠️ Requires browser testing
- Room input validation
- Wall hexagon interactions
- Progressive disclosure flow
- Audio generation from positions
- Breadcrumb navigation
- Result display with metadata

## Usage Example

1. User visits `browse.html`
2. Enters room number `42`
3. Clicks "Enter Room" button
4. Hexagon appears with 4 walls
5. User clicks "Wall 2" (top-right polygon)
6. Grid of 5 shelves appears (Shelf 0 - Shelf 4)
7. User clicks "Shelf 3"
8. Grid of 32 albums appears (Album 0 - Album 31)
9. User clicks "Album 15"
10. Grid of 15 tracks appears (Track 0 - Track 14)
11. User clicks "Track 7"
12. System calculates:
    - `index = reconstructIndexFromPosition({42n, 2, 3, 15, 7})`
    - `index = 42n * 9600n + 2 * 2400n + 3 * 480n + 15 * 15n + 7`
    - `index = 403200n + 4800n + 1440n + 225n + 7 = 409672n`
13. WASM generates audio from index `409672`
14. Metadata extracted: genre, artist, album, track labels
15. SVG cover generated
16. Audio plays, metadata and cover displayed

### Integration Points

### With WASM Module

- `audioIndexWasm.js` - Wrapper for C++ audio generation
- `wasm.module.reconstructIndex(roomStr, wall, shelf, album, track)` - Convert position to base64 index
- `wasm.module.calculatePosition(base64Index)` - Extract position from base64 index (returns JSON)
- `wasm.module.getMetadata(base64Index)` - Extract metadata from index

### With Result Handler
- `resultHandler.js` - Displays generated audio track
- Receives metadata with position information
- Shows genre/artist/album/track and cover art

### With Audio Player
- Browser native `<audio>` element
- WAV file generated as data URL
- Autoplay on track selection

## Future Enhancements

- [ ] Add "Back" navigation buttons at each level
- [ ] Smooth animated transitions between sections
- [ ] URL hash navigation (bookmark specific positions)
- [ ] Random position generator ("Surprise me" button)
- [ ] Search within current room
- [ ] Visual preview of album covers before selection
- [ ] Keyboard navigation support (arrow keys, enter)
- [ ] History/favorites system using localStorage

## File Checklist

### Created

- ✅ `cpp/include/LibraryPosition.h`
- ✅ `cpp/src/LibraryPosition.cpp`
- ✅ `docs/BROWSE_HIERARCHICAL_NAV.md` (this file)

### Modified

- ✅ `cpp/include/IndexMetadata.h` - Added position field
- ✅ `cpp/src/IndexMetadata.cpp` - Calculate position in both overloads
- ✅ `cpp/tests/test_main.cpp` - Added 8 LibraryPosition tests
- ✅ `cpp/wasm/wasm_bindings.cpp` - Added WASM bindings for position functions (calculatePosition, reconstructIndex)
- ✅ `cpp/wasm/CMakeLists.txt` - Added LibraryPosition.cpp to WASM build
- ✅ `docs/browse.html` - Complete restructure with sections
- ✅ `docs/js/browse.js` - Rewritten with hierarchical logic using WASM position functions
- ✅ `docs/css/browse.css` - Updated for new UI elements

### Removed

- ❌ `docs/js/positionEncoder.js` - No longer needed (WASM handles position calculations)

## Mathematical Guarantee

The position system guarantees:
1. **Determinism**: Same input always produces same position
2. **Bijection**: Every index maps to exactly one position (no collisions)
3. **Completeness**: All positions in range are reachable
4. **Consistency**: C++ and JavaScript implementations produce identical results

Formula for index reconstruction:
```
index = room × ITEMS_PER_ROOM 
        + wall × ITEMS_PER_WALL
        + shelf × ITEMS_PER_SHELF
        + album × ITEMS_PER_ALBUM
        + track

where:
  ITEMS_PER_ROOM = 9600
  ITEMS_PER_WALL = 2400
  ITEMS_PER_SHELF = 480
  ITEMS_PER_ALBUM = 15
```

## Notes

- Room numbers can be any positive integer (BigInt support)
- Wall numbers are 0-indexed (0, 1, 2, 3) representing 4 genres
- All navigation is client-side (no server required)
- Audio generation happens in-browser via WASM
- Position metadata is now part of every generated track

---

**Implementation Date:** 2025  
**Status:** Complete (pending browser testing)  
**Tests:** All C++ tests passing (53/53)
