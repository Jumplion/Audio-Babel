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

    // Encode room number as base64
    // Room 0 special case: use empty string for simplicity
    if (roomNumber == 0) {
        pos.room = "";
    } else {
        std::vector<uint8_t> roomBytes;
        boost::multiprecision::export_bits(roomNumber, std::back_inserter(roomBytes), 8, true);
        pos.room = Utilities::encodeBase64Url(roomBytes);
    }

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
    // Decode room from base64
    std::vector<uint8_t> roomBytes = Utilities::decodeBase64Url(pos.room);

    cpp_int roomNumber = 0;
    if (!roomBytes.empty()) {
        boost::multiprecision::import_bits(roomNumber, roomBytes.begin(), roomBytes.end(), 8, true);
    }

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
