#include "../include/LibraryPosition.h"

#include <iomanip>
#include <sstream>


namespace AudioBabel {

using namespace LibraryConstants;

auto calculateLibraryPosition(const cpp_int& index) -> LibraryPosition {
    LibraryPosition pos;

    // Use modular arithmetic to determine position in hierarchy
    // This ensures every unique index maps to exactly one position

    // Calculate room number (infinite rooms possible)
    pos.room = index / ITEMS_PER_ROOM;

    // Calculate position within the room
    cpp_int remainder = index % ITEMS_PER_ROOM;

    // Wall (0-3): 4 walls per room
    pos.wall = static_cast<uint8_t>((remainder / ITEMS_PER_WALL) % WALLS_PER_ROOM);

    remainder = remainder % ITEMS_PER_WALL;

    // Shelf (0-4): 5 shelves per wall
    pos.shelf = static_cast<uint8_t>((remainder / ITEMS_PER_SHELF) % SHELVES_PER_WALL);

    remainder = remainder % ITEMS_PER_SHELF;

    // Album (0-31): 32 albums per shelf
    pos.album = static_cast<uint8_t>((remainder / ITEMS_PER_ALBUM) % ALBUMS_PER_SHELF);

    // Track (0-14): 15 tracks per album
    pos.track = static_cast<uint8_t>(remainder % TRACKS_PER_ALBUM);

    return pos;
}

auto reconstructIndexFromPosition(const LibraryPosition& pos) -> cpp_int {
    // Inverse operation: reconstruct index from position
    // This allows bidirectional navigation between index and position

    cpp_int index = pos.room * cpp_int(ITEMS_PER_ROOM);
    index += cpp_int(pos.wall) * ITEMS_PER_WALL;
    index += cpp_int(pos.shelf) * ITEMS_PER_SHELF;
    index += cpp_int(pos.album) * ITEMS_PER_ALBUM;
    index += cpp_int(pos.track);

    return index;
}

auto LibraryPosition::toString() const -> std::string {
    std::ostringstream oss;
    oss << "Room " << room << ", Wall " << static_cast<int>(wall) << ", Shelf " << static_cast<int>(shelf) << ", Album " << static_cast<int>(album)
        << ", Track " << static_cast<int>(track);
    return oss.str();
}

} // namespace AudioBabel
