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
 * - Neighbor dissimilarity (adjacent positions produce scattered indices)
 *
 * Migrated to Catch2 v3 framework.
 */

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <sstream>
#include <vector>

#include "test_common.h"

using boost::multiprecision::cpp_int;
using namespace LibraryConstants;

TEST_CASE("LibraryPosition: calculateLibraryPosition and roundtrip for representative indexes", "[library_position][calculation][roundtrip]") {
    struct Case {
        cpp_int index;
        bool    roomEmpty;
    };

    std::vector<Case> cases = {
        {42,    true},   // room 0
        {12345, false},  // room 1
        {9600,  false},  // first index in room 1
    };

    for (const auto& c : cases) {
        INFO("Testing index: " << c.index);
        auto pos = calculateLibraryPosition(c.index);

        REQUIRE(pos.room.empty() == c.roomEmpty);

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

TEST_CASE("LibraryPosition: cross-module consistency with IndexMetadata and full roundtrip",
          "[library_position][metadata][comprehensive][roundtrip]") {
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

        auto meta = IndexMetadata::extractMetadataFromIndex(index);
        auto pos_direct = calculateLibraryPosition(index);

        REQUIRE(meta.position.room  == pos_direct.room);
        REQUIRE(meta.position.wall  == pos_direct.wall);
        REQUIRE(meta.position.shelf == pos_direct.shelf);
        REQUIRE(meta.position.album == pos_direct.album);
        REQUIRE(meta.position.track == pos_direct.track);

        cpp_int reconstructed = reconstructIndexFromPosition(meta.position);
        REQUIRE(reconstructed == index);
    }
}

TEST_CASE("LibraryPosition: zero index maps to room 0", "[library_position][edge_case]") {
    cpp_int index = 0;
    auto    pos   = calculateLibraryPosition(index);

    // With offset scrambling, wall/shelf/album/track are no longer necessarily 0,
    // but the index must round-trip and must land in room 0.
    REQUIRE(pos.room == "");
    REQUIRE(pos.wall  < WALLS_PER_ROOM);
    REQUIRE(pos.shelf < SHELVES_PER_WALL);
    REQUIRE(pos.album < ALBUMS_PER_SHELF);
    REQUIRE(pos.track < TRACKS_PER_ALBUM);

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
    for (int i = 0; i < 100; i++) {
        INFO("Testing consecutive indexes " << i << " and " << (i + 1));

        cpp_int idx1 = i;
        cpp_int idx2 = i + 1;

        auto pos1 = calculateLibraryPosition(idx1);
        auto pos2 = calculateLibraryPosition(idx2);

        bool different = (pos1.room != pos2.room) || (pos1.wall != pos2.wall) || (pos1.shelf != pos2.shelf) || (pos1.album != pos2.album) ||
                         (pos1.track != pos2.track);

        REQUIRE(different);
    }
}

TEST_CASE("LibraryPosition: perfect bijection - all positions in range are reachable", "[library_position][bijection]") {
    SECTION("All tracks in first album produce distinct in-range indices") {
        std::vector<cpp_int> indices;
        indices.reserve(TRACKS_PER_ALBUM);
        for (uint8_t track = 0; track < TRACKS_PER_ALBUM; ++track) {
            LibraryPosition pos;
            pos.room  = "";
            pos.wall  = 0;
            pos.shelf = 0;
            pos.album = 0;
            pos.track = track;

            cpp_int idx = reconstructIndexFromPosition(pos);
            REQUIRE(idx >= 0);
            REQUIRE(idx < cpp_int(ITEMS_PER_ROOM)); // stays in room 0
            indices.push_back(idx);
        }
        // All 15 must be distinct (bijection property)
        std::vector<cpp_int> sorted_indices = indices;
        std::sort(sorted_indices.begin(), sorted_indices.end());
        for (size_t i = 1; i < sorted_indices.size(); ++i) {
            REQUIRE(sorted_indices[i] != sorted_indices[i - 1]);
        }
    }

    SECTION("First position of each album in first shelf produces distinct in-range indices") {
        std::vector<cpp_int> indices;
        indices.reserve(ALBUMS_PER_SHELF);
        for (uint8_t album = 0; album < ALBUMS_PER_SHELF; ++album) {
            LibraryPosition pos;
            pos.room  = "";
            pos.wall  = 0;
            pos.shelf = 0;
            pos.album = album;
            pos.track = 0;

            cpp_int idx = reconstructIndexFromPosition(pos);
            REQUIRE(idx >= 0);
            REQUIRE(idx < cpp_int(ITEMS_PER_ROOM)); // stays in room 0
            indices.push_back(idx);
        }
        // All 32 must be distinct (bijection property)
        std::vector<cpp_int> sorted_indices = indices;
        std::sort(sorted_indices.begin(), sorted_indices.end());
        for (size_t i = 1; i < sorted_indices.size(); ++i) {
            REQUIRE(sorted_indices[i] != sorted_indices[i - 1]);
        }
    }
}

TEST_CASE("LibraryPosition: boundary indices round-trip correctly", "[library_position][boundary]") {
    // These boundary values exercise room-0 edges. With offset scrambling the
    // wall/shelf/album/track are permuted, but every index must round-trip and
    // stay in room 0 (all values < ITEMS_PER_ROOM).
    std::vector<cpp_int> boundaries = {14, 15, 479, 480, 2399, 2400, 9599};

    for (const auto& idx : boundaries) {
        INFO("Boundary index: " << idx);
        auto pos = calculateLibraryPosition(idx);

        REQUIRE(pos.room  == "");
        REQUIRE(pos.wall  < WALLS_PER_ROOM);
        REQUIRE(pos.shelf < SHELVES_PER_WALL);
        REQUIRE(pos.album < ALBUMS_PER_SHELF);
        REQUIRE(pos.track < TRACKS_PER_ALBUM);
        REQUIRE(reconstructIndexFromPosition(pos) == idx);
    }
}

TEST_CASE("LibraryPosition: adjacent tracks produce scattered indices", "[library_position][scramble]") {
    std::vector<cpp_int> indices;
    indices.reserve(TRACKS_PER_ALBUM);
    for (uint8_t track = 0; track < TRACKS_PER_ALBUM; ++track) {
        LibraryPosition pos;
        pos.room  = "";
        pos.wall  = 0;
        pos.shelf = 0;
        pos.album = 0;
        pos.track = track;
        indices.push_back(reconstructIndexFromPosition(pos));
    }

    // No two consecutive tracks (by track number) should map to adjacent indices.
    for (size_t i = 0; i + 1 < indices.size(); ++i) {
        cpp_int diff = indices[i + 1] > indices[i] ? indices[i + 1] - indices[i] : indices[i] - indices[i + 1];
        INFO("Track " << i << " → " << indices[i] << ", track " << (i + 1) << " → " << indices[i + 1] << ", diff = " << diff);
        REQUIRE(diff > cpp_int(1));
    }

    // The spread across 15 well-scattered points in [0, 9600) should be large.
    auto mn = *std::min_element(indices.begin(), indices.end());
    auto mx = *std::max_element(indices.begin(), indices.end());
    REQUIRE((mx - mn) > cpp_int(1000));
}

TEST_CASE("LibraryPosition: reconstructIndexFromPosition with out-of-range values", "[library_position][edge_case][validation]") {
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
        cpp_int index = 999999; // arbitrary large index
        auto    pos   = calculateLibraryPosition(index);

        REQUIRE(pos.wall  < WALLS_PER_ROOM);
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

        REQUIRE_THROWS_AS(reconstructIndexFromPosition(pos), std::invalid_argument);
    }
}
