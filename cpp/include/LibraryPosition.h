#ifndef LIBRARY_POSITION_H
#define LIBRARY_POSITION_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <string>

namespace AudioBabel {

using boost::multiprecision::cpp_int;

// Deterministic, perfectly bijective mapping from an audio index to a
// record-shop hierarchy (room -> wall -> shelf -> album -> track). Each room
// holds 9,600 tracks (4 x 5 x 32 x 15); see cpp/include/README.md / root
// README for the hierarchy table and bijection details.
struct LibraryPosition {
    std::string room;  // base64-encoded room identifier (high-order bits)
    uint8_t     wall;  // 0-3
    uint8_t     shelf; // 0-4
    uint8_t     album; // 0-31
    uint8_t     track; // 0-14
};

// index % 9600 (Feistel-unscrambled) decomposes into wall/shelf/album/track;
// index / 9600 becomes the base64-encoded room identifier (0 -> empty string).
auto calculateLibraryPosition(const cpp_int& index) -> LibraryPosition;

// Inverse of calculateLibraryPosition(): reconstructIndexFromPosition(calculateLibraryPosition(x)) == x.
// Throws std::invalid_argument if position fields are out of range.
auto reconstructIndexFromPosition(const LibraryPosition& pos) -> cpp_int;

// Capacity at each level of the library hierarchy.
namespace LibraryConstants {
    constexpr uint32_t TRACKS_PER_ALBUM = 15;
    constexpr uint32_t ALBUMS_PER_SHELF = 32;
    constexpr uint32_t SHELVES_PER_WALL = 5;
    constexpr uint32_t WALLS_PER_ROOM   = 4;

    constexpr uint32_t ITEMS_PER_ALBUM = TRACKS_PER_ALBUM;
    constexpr uint32_t ITEMS_PER_SHELF = ALBUMS_PER_SHELF * ITEMS_PER_ALBUM; // 480
    constexpr uint32_t ITEMS_PER_WALL  = SHELVES_PER_WALL * ITEMS_PER_SHELF; // 2,400
    constexpr uint32_t ITEMS_PER_ROOM  = WALLS_PER_ROOM * ITEMS_PER_WALL;    // 9,600
} // namespace LibraryConstants

} // namespace AudioBabel

#endif // LIBRARY_POSITION_H
