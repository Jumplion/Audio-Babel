#include "../include/LibraryPosition.h"

#include <iomanip>
#include <sstream>

#include "../include/Utilities.h"

namespace AudioBabel {

using namespace LibraryConstants;

auto calculateLibraryPosition(const cpp_int& index) -> LibraryPosition {
    LibraryPosition pos;

    // Split index into two parts:
    // 1. Room number (index / 9600) - will be base64-encoded
    // 2. Position within room (index % 9600) - encodes wall/shelf/album/track

    cpp_int roomNumber = index / ITEMS_PER_ROOM;
    cpp_int withinRoom = index % ITEMS_PER_ROOM;

    // Encode the room number with the bijective base-64 used everywhere else.
    // Room 0 naturally maps to the empty string (indexToB64(0) == "").
    pos.room = Utilities::indexToB64(roomNumber);

    // Calculate hierarchical position within the room using modular arithmetic
    pos.wall = static_cast<uint8_t>((withinRoom / ITEMS_PER_WALL) % WALLS_PER_ROOM);

    cpp_int remainder = withinRoom % ITEMS_PER_WALL;
    pos.shelf         = static_cast<uint8_t>((remainder / ITEMS_PER_SHELF) % SHELVES_PER_WALL);

    remainder = remainder % ITEMS_PER_SHELF;
    pos.album = static_cast<uint8_t>((remainder / ITEMS_PER_ALBUM) % ALBUMS_PER_SHELF);

    pos.track = static_cast<uint8_t>(remainder % TRACKS_PER_ALBUM);

    return pos;
}

auto reconstructIndexFromPosition(const LibraryPosition& pos) -> cpp_int {
    // Validate position fields are within documented ranges
    if (pos.wall >= LibraryConstants::WALLS_PER_ROOM) {
        throw std::invalid_argument("wall out of range: " + std::to_string(pos.wall) + " (max " +
                                    std::to_string(LibraryConstants::WALLS_PER_ROOM - 1) + ")");
    }
    if (pos.shelf >= LibraryConstants::SHELVES_PER_WALL) {
        throw std::invalid_argument("shelf out of range: " + std::to_string(pos.shelf) + " (max " +
                                    std::to_string(LibraryConstants::SHELVES_PER_WALL - 1) + ")");
    }
    if (pos.album >= LibraryConstants::ALBUMS_PER_SHELF) {
        throw std::invalid_argument("album out of range: " + std::to_string(pos.album) + " (max " +
                                    std::to_string(LibraryConstants::ALBUMS_PER_SHELF - 1) + ")");
    }
    if (pos.track >= LibraryConstants::TRACKS_PER_ALBUM) {
        throw std::invalid_argument("track out of range: " + std::to_string(pos.track) + " (max " +
                                    std::to_string(LibraryConstants::TRACKS_PER_ALBUM - 1) + ")");
    }

    // Decode room from the bijective base-64 string (empty string -> 0).
    cpp_int roomNumber = Utilities::b64ToIndex(pos.room);

    // Reconstruct index from room number and hierarchical position
    cpp_int index = roomNumber * cpp_int(ITEMS_PER_ROOM);
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
