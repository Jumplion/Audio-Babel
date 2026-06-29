#ifndef LIBRARY_POSITION_H
#define LIBRARY_POSITION_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <string>

namespace AudioBabel {

using boost::multiprecision::cpp_int;

/**
 * @struct LibraryPosition
 * @brief Hierarchical position in the "Speaker of Babel" library.
 * 
 * LibraryPosition represents a deterministic, lossless, perfectly bijective
 * mapping from audio indexes to a virtual record-shop hierarchy (room → wall →
 * shelf → album → track). Each room holds exactly 9,600 tracks (4 × 5 × 32 × 15);
 * the base64-encoded room identifier gives unlimited rooms. The per-level ranges
 * are documented on the fields below; see cpp/include/README.md and the root
 * README for the hierarchy table and bijection details.
 *
 * @see calculateLibraryPosition for computing position from index
 * @see reconstructIndexFromPosition for inverse operation
 */
struct LibraryPosition {
    std::string room;  ///< Base64-encoded room identifier (high-order bits)
    uint8_t     wall;  ///< Wall number (0-3)
    uint8_t     shelf; ///< Shelf number within wall (0-4)
    uint8_t     album; ///< Album number within shelf (0-31)
    uint8_t     track; ///< Track number within album (0-14)
};

/**
 * @brief Calculate hierarchical position from a big integer index.
 * 
 * Deterministically maps an audio index to its unique position in the library
 * hierarchy using modular arithmetic. The lower 9600 possible values (index % 9600)
 * encode wall/shelf/album/track, while the quotient (index / 9600) becomes the
 * base64-encoded room identifier.
 * 
 * @param index Big integer audio index
 * @return LibraryPosition structure with all hierarchy levels populated
 * 
 * @par Algorithm
 * 1. Compute remainder = index % 9600 for hierarchical subdivision
 * 2. Unscramble remainder via a room-keyed Feistel permutation to get linearOffset
 * 3. Extract track (linearOffset % 15), album, shelf, wall via successive division
 * 4. Compute room = base64(index / 9600), with room 0 encoded as empty string
 *
 * @note Every unique index maps to exactly one position (injective function)
 * 
 * @see reconstructIndexFromPosition for the inverse operation
 * @see LibraryConstants for hierarchy structure constants
 */
auto calculateLibraryPosition(const cpp_int& index) -> LibraryPosition;

/**
 * @brief Reconstruct an index from a hierarchical position.
 * 
 * This is the inverse of calculateLibraryPosition(), enabling navigation by
 * position coordinates. Given a valid LibraryPosition, reconstructs the unique
 * big integer index that maps to that position.
 * 
 * @param pos Library position structure with room, wall, shelf, album, track
 * @return Big integer index corresponding to the position
 * @throws std::invalid_argument if position fields are out of valid ranges
 * 
 * @par Algorithm
 * 1. Decode room base64 string to big integer (with empty string = 0)
 * 2. Compute hierarchy_offset = wall×2400 + shelf×480 + album×15 + track
 * 3. Scramble hierarchy_offset via the room-keyed Feistel permutation
 * 4. Return room_value × 9600 + scrambled_offset
 * 
 * @note This function guarantees: reconstructIndexFromPosition(calculateLibraryPosition(x)) == x
 * 
 * @see calculateLibraryPosition for the forward operation
 */
auto reconstructIndexFromPosition(const LibraryPosition& pos) -> cpp_int;

/**
 * @namespace LibraryConstants
 * @brief Constants defining the hierarchical library structure.
 * 
 * These constants specify the capacity at each level of the library hierarchy
 * and are used throughout position calculation and reconstruction.
 */
namespace LibraryConstants {
    constexpr uint32_t TRACKS_PER_ALBUM = 15; ///< Number of tracks per album (0-14)
    constexpr uint32_t ALBUMS_PER_SHELF = 32; ///< Number of albums per shelf (0-31)
    constexpr uint32_t SHELVES_PER_WALL = 5;  ///< Number of shelves per wall (0-4)
    constexpr uint32_t WALLS_PER_ROOM   = 4;  ///< Number of walls per room (0-3)

    // Derived constants for position calculations
    constexpr uint32_t ITEMS_PER_ALBUM = TRACKS_PER_ALBUM;                   ///< 15 items per album
    constexpr uint32_t ITEMS_PER_SHELF = ALBUMS_PER_SHELF * ITEMS_PER_ALBUM; ///< 480 items per shelf
    constexpr uint32_t ITEMS_PER_WALL  = SHELVES_PER_WALL * ITEMS_PER_SHELF; ///< 2,400 items per wall
    constexpr uint32_t ITEMS_PER_ROOM  = WALLS_PER_ROOM * ITEMS_PER_WALL;    ///< 9,600 items per room (total)
} // namespace LibraryConstants

} // namespace AudioBabel

#endif // LIBRARY_POSITION_H
