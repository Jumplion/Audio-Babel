/**
 * @file test_library_position.cpp
 * @brief Unit tests for LibraryPosition calculation and reconstruction.
 * 
 * Tests the LibraryPosition system including:
 * - Position calculation from indexes
 * - Index reconstruction from positions
 * - Bijection properties (one-to-one mapping)
 * - Boundary value handling
 * - Constants validation
 * - Uniqueness guarantees
 * 
 * Migrated to Catch2 v3 framework.
 */

#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <vector>

#include "test_common.h"

using boost::multiprecision::cpp_int;
using namespace LibraryConstants;

TEST_CASE("LibraryPosition: calculateLibraryPosition and roundtrip for representative indexes", "[library_position][calculation][roundtrip]") {
    // Table-driven replacement for what used to be four near-identical
    // single-index tests (42, 12345, 9600, 50000 [50000 now covered by the
    // comprehensive roundtrip list below]).
    struct Case {
        cpp_int index;
        bool    roomEmpty;
        uint8_t wall;
        uint8_t shelf;
        uint8_t album;
        uint8_t track;
    };

    std::vector<Case> cases = {
        // index 42: room 0 ("" ), wall=(42/2400)%4=0, shelf=(42/480)%5=0, album=(42/15)%32=2, track=42%15=12
        {42, true, 0, 0, 2, 12},
        // index 12345: room 1 (non-empty), wall=1, shelf=0, album=23, track=0
        {12345, false, 1, 0, 23, 0},
        // index 9600: first index in room 1
        {9600, false, 0, 0, 0, 0},
    };

    for (const auto& c : cases) {
        INFO("Testing index: " << c.index);
        auto pos = calculateLibraryPosition(c.index);

        REQUIRE(pos.room.empty() == c.roomEmpty);
        REQUIRE(pos.wall == c.wall);
        REQUIRE(pos.shelf == c.shelf);
        REQUIRE(pos.album == c.album);
        REQUIRE(pos.track == c.track);

        cpp_int reconstructed = reconstructIndexFromPosition(pos);
        REQUIRE(c.index == reconstructed);
    }
}

TEST_CASE("LibraryPosition: IndexMetadata includes position field", "[library_position][metadata]") {
    // Create a simple base64 index
    std::vector<uint8_t> bytes = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    cpp_int              raw   = 0;
    boost::multiprecision::import_bits(raw, bytes.begin(), bytes.end(), 8, true);
    std::string base64 = Utilities::indexToB64(raw);

    auto meta = IndexMetadata::extractMetadataFromIndex(base64);

    // Check that position was calculated
    REQUIRE(!meta.genre.empty());
    REQUIRE(!meta.artist.empty());
    REQUIRE(!meta.album.empty());
    REQUIRE(!meta.track.empty());
    REQUIRE(!meta.position.room.empty());
}

TEST_CASE("LibraryPosition: cross-module consistency with IndexMetadata and full roundtrip", "[library_position][metadata][comprehensive][roundtrip]") {
    // Comprehensive list of indexes covering edge cases, boundaries, and large
    // values. This subsumes what used to be two separate near-duplicate tests
    // ("cpp_int overload includes position", "complete roundtrip for various
    // patterns") plus the standalone "large index roundtrip" (50000) case.
    std::vector<cpp_int> test_indexes = {
        0,                         // Origin
        1,                         // First track
        14,                        // Last track in first album
        15,                        // First track in second album
        16,                        // Album boundary
        479,                       // Last track in first shelf
        480,                       // First track in second shelf
        481,                       // Shelf boundary
        2399,                      // Last track in first wall
        2400,                      // First track in second wall
        2401,                      // Wall boundary
        9599,                      // Last track in first room
        9600,                      // First track in second room
        9601,                      // Room boundary
        12345,                     // Arbitrary value
        50000,                     // Arbitrary value
        67890,                     // Arbitrary value
        123456789,                 // Large value
        cpp_int("999999999999999") // Very large value
    };

    for (const auto& index : test_indexes) {
        INFO("Testing index: " << index);

        // Extract metadata using IndexMetadata (which internally uses LibraryPosition)
        auto meta = IndexMetadata::extractMetadataFromIndex(index);

        // Calculate position directly using LibraryPosition function
        auto pos_direct = calculateLibraryPosition(index);

        // Verify perfect consistency across all position fields
        REQUIRE(meta.position.room == pos_direct.room);
        REQUIRE(meta.position.wall == pos_direct.wall);
        REQUIRE(meta.position.shelf == pos_direct.shelf);
        REQUIRE(meta.position.album == pos_direct.album);
        REQUIRE(meta.position.track == pos_direct.track);

        // Additionally verify that reconstructing the index from the position
        // in the metadata gives us back the original index
        cpp_int reconstructed = reconstructIndexFromPosition(meta.position);
        REQUIRE(reconstructed == index);
    }
}

TEST_CASE("LibraryPosition: zero index maps to origin", "[library_position][edge_case]") {
    cpp_int index = 0;
    auto    pos   = calculateLibraryPosition(index);

    REQUIRE(pos.room == "");
    REQUIRE(pos.wall == 0);
    REQUIRE(pos.shelf == 0);
    REQUIRE(pos.album == 0);
    REQUIRE(pos.track == 0);

    // Verify roundtrip
    cpp_int reconstructed = reconstructIndexFromPosition(pos);
    REQUIRE(index == reconstructed);
}

TEST_CASE("LibraryPosition: constants validation", "[library_position][constants]") {
    REQUIRE(TRACKS_PER_ALBUM == 15);
    REQUIRE(ALBUMS_PER_SHELF == 32);
    REQUIRE(SHELVES_PER_WALL == 5);
    REQUIRE(WALLS_PER_ROOM == 4);
    REQUIRE(ITEMS_PER_ALBUM == 15);
    REQUIRE(ITEMS_PER_SHELF == 480);
    REQUIRE(ITEMS_PER_WALL == 2400);
    REQUIRE(ITEMS_PER_ROOM == 9600);
}

TEST_CASE("LibraryPosition: uniqueness - consecutive indexes map to different positions", "[library_position][uniqueness]") {
    // Test that consecutive indexes produce different positions
    for (int i = 0; i < 100; i++) {
        INFO("Testing consecutive indexes " << i << " and " << (i + 1));

        cpp_int idx1 = i;
        cpp_int idx2 = i + 1;

        auto pos1 = calculateLibraryPosition(idx1);
        auto pos2 = calculateLibraryPosition(idx2);

        // At least one field should be different
        bool different = (pos1.room != pos2.room) || (pos1.wall != pos2.wall) || (pos1.shelf != pos2.shelf) || (pos1.album != pos2.album) ||
                         (pos1.track != pos2.track);

        REQUIRE(different);
    }
}

TEST_CASE("LibraryPosition: perfect bijection - all positions in range are reachable", "[library_position][bijection]") {
    SECTION("All tracks in first album are reachable") {
        // Test all positions in first album (room 0, wall 0, shelf 0, album 0)
        for (uint8_t track = 0; track < TRACKS_PER_ALBUM; track++) {
            INFO("Testing track: " << (int) track);

            LibraryPosition pos;
            pos.room  = ""; // Room 0 is empty string
            pos.wall  = 0;
            pos.shelf = 0;
            pos.album = 0;
            pos.track = track;

            cpp_int reconstructed = reconstructIndexFromPosition(pos);
            REQUIRE(reconstructed == track);
        }
    }

    SECTION("First position of each album in first shelf is reachable") {
        // Test first position of each album in first shelf (room 0, wall 0, shelf 0)
        for (uint8_t album = 0; album < ALBUMS_PER_SHELF; album++) {
            INFO("Testing album: " << (int) album);

            LibraryPosition pos;
            pos.room  = ""; // Room 0 is empty string
            pos.wall  = 0;
            pos.shelf = 0;
            pos.album = album;
            pos.track = 0;

            cpp_int reconstructed = reconstructIndexFromPosition(pos);
            cpp_int expected      = album * ITEMS_PER_ALBUM;

            REQUIRE(reconstructed == expected);
        }
    }
}

TEST_CASE("LibraryPosition: boundary values produce correct positions", "[library_position][boundary]") {
    struct Case {
        cpp_int index;
        uint8_t wall;
        uint8_t shelf;
        uint8_t album;
        uint8_t track;
    };

    std::vector<Case> cases = {
        {14, 0, 0, 0, 14},    // Last track of first album
        {15, 0, 0, 1, 0},     // First track of second album
        {479, 0, 0, 31, 14},  // Last track of first shelf
        {480, 0, 1, 0, 0},    // First track of second shelf
        {2399, 0, 4, 31, 14}, // Last track of first wall
        {2400, 1, 0, 0, 0},   // First track of second wall
        {9599, 3, 4, 31, 14}, // Last track of first room
    };

    for (const auto& c : cases) {
        INFO("Testing boundary index: " << c.index);
        auto pos = calculateLibraryPosition(c.index);

        REQUIRE(pos.room == "");
        REQUIRE(pos.wall == c.wall);
        REQUIRE(pos.shelf == c.shelf);
        REQUIRE(pos.album == c.album);
        REQUIRE(pos.track == c.track);
    }
}

TEST_CASE("LibraryPosition: reconstructIndexFromPosition with out-of-range values", "[library_position][edge_case][validation]") {
    // The header documents: "@throws std::invalid_argument if position fields are out of valid ranges"
    // The implementation now enforces this contract.
    //
    // JavaScript layer (browse.js) also validates and clamps input values before calling WASM,
    // but the C++ layer now rejects invalid inputs as a defence-in-depth measure.

    SECTION("track out of range (15, max is 14)") {
        LibraryPosition pos;
        pos.room  = ""; // room 0
        pos.wall  = 0;
        pos.shelf = 0;
        pos.album = 0;
        pos.track = 15; // OUT OF RANGE

        REQUIRE_THROWS_AS(reconstructIndexFromPosition(pos), std::invalid_argument);
    }

    SECTION("album out of range (32, max is 31)") {
        LibraryPosition pos;
        pos.room  = "";
        pos.wall  = 0;
        pos.shelf = 0;
        pos.album = 32; // OUT OF RANGE
        pos.track = 0;

        REQUIRE_THROWS_AS(reconstructIndexFromPosition(pos), std::invalid_argument);
    }

    SECTION("shelf out of range (5, max is 4)") {
        LibraryPosition pos;
        pos.room  = "";
        pos.wall  = 0;
        pos.shelf = 5; // OUT OF RANGE
        pos.album = 0;
        pos.track = 0;

        REQUIRE_THROWS_AS(reconstructIndexFromPosition(pos), std::invalid_argument);
    }

    SECTION("wall out of range (4, max is 3)") {
        LibraryPosition pos;
        pos.room  = "";
        pos.wall  = 4; // OUT OF RANGE
        pos.shelf = 0;
        pos.album = 0;
        pos.track = 0;

        REQUIRE_THROWS_AS(reconstructIndexFromPosition(pos), std::invalid_argument);
    }

    SECTION("multiple out-of-range fields") {
        LibraryPosition pos;
        pos.room  = "";
        pos.wall  = 4;  // overflow
        pos.shelf = 5;  // overflow
        pos.album = 32; // overflow
        pos.track = 15; // overflow

        // wall is checked first
        REQUIRE_THROWS_AS(reconstructIndexFromPosition(pos), std::invalid_argument);
    }

    SECTION("extreme out-of-range value (uint8_t max = 255)") {
        LibraryPosition pos;
        pos.room  = "";
        pos.wall  = 255; // extremely out of range
        pos.shelf = 0;
        pos.album = 0;
        pos.track = 0;

        REQUIRE_THROWS_AS(reconstructIndexFromPosition(pos), std::invalid_argument);
    }

    SECTION("Forward calculation never produces out-of-range values") {
        // Forward calculation should always produce valid ranges due to modulo operations
        cpp_int index = 999999; // arbitrary large index
        auto    pos   = calculateLibraryPosition(index);

        REQUIRE(pos.wall < WALLS_PER_ROOM);
        REQUIRE(pos.shelf < SHELVES_PER_WALL);
        REQUIRE(pos.album < ALBUMS_PER_SHELF);
        REQUIRE(pos.track < TRACKS_PER_ALBUM);
    }

    SECTION("Invalid base64 in room field throws exception") {
        LibraryPosition pos;
        pos.room  = "!!!INVALID!!!"; // Invalid base64
        pos.wall  = 0;
        pos.shelf = 0;
        pos.album = 0;
        pos.track = 0;

        // Expected: b64ToIndex should throw on invalid characters
        REQUIRE_THROWS_AS(reconstructIndexFromPosition(pos), std::invalid_argument);
    }
}
