#ifndef LIBRARY_POSITION_H
#define LIBRARY_POSITION_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <string>

namespace AudioBabel {

using boost::multiprecision::cpp_int;

/**
 * Represents a unique position in the library hierarchy.
 * Room → Wall (4 per room) → Shelf (5 per wall) → Album (32 per shelf) → Track (15 per album)
 * 
 * This provides a deterministic, collision-free mapping from any audio index
 * to a unique location in the record shop structure.
 */
struct LibraryPosition {
    cpp_int room;  // Infinite rooms (0, 1, 2, ...)
    uint8_t wall;  // 0-3 (4 walls per room, representing "genre")
    uint8_t shelf; // 0-4 (5 shelves/longboxes per wall, representing "artist")
    uint8_t album; // 0-31 (32 albums per shelf)
    uint8_t track; // 0-14 (15 tracks per album)

    /**
     * Convert the position to a human-readable string.
     * Format: "Room X, Wall Y, Shelf Z, Album A, Track T"
     */
    auto toString() const -> std::string;
};

/**
 * Calculate hierarchical position from an index using modular arithmetic.
 * Every unique index maps to exactly one position.
 * 
 * @param index The audio index as a big integer
 * @return LibraryPosition structure with room, wall, shelf, album, and track numbers
 */
auto calculateLibraryPosition(const cpp_int& index) -> LibraryPosition;

/**
 * Reconstruct an index from a hierarchical position.
 * This is the inverse of calculateLibraryPosition - allows navigation by position.
 * 
 * @param pos The library position structure
 * @return The audio index corresponding to that position
 */
auto reconstructIndexFromPosition(const LibraryPosition& pos) -> cpp_int;

// Constants for hierarchy structure
namespace LibraryConstants {
    constexpr uint32_t TRACKS_PER_ALBUM = 15;
    constexpr uint32_t ALBUMS_PER_SHELF = 32;
    constexpr uint32_t SHELVES_PER_WALL = 5;
    constexpr uint32_t WALLS_PER_ROOM   = 4;

    // Derived constants for position calculations
    constexpr uint32_t ITEMS_PER_ALBUM = TRACKS_PER_ALBUM;                   // 15
    constexpr uint32_t ITEMS_PER_SHELF = ALBUMS_PER_SHELF * ITEMS_PER_ALBUM; // 480
    constexpr uint32_t ITEMS_PER_WALL  = SHELVES_PER_WALL * ITEMS_PER_SHELF; // 2,400
    constexpr uint32_t ITEMS_PER_ROOM  = WALLS_PER_ROOM * ITEMS_PER_WALL;    // 9,600
} // namespace LibraryConstants

} // namespace AudioBabel

#endif // LIBRARY_POSITION_H
