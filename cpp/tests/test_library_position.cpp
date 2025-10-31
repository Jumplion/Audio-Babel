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
 */

#include "test_common.h"

/**
 * @brief Register all library position-related tests with the test runner.
 * @param runner TestRunner instance to register tests with
 */
void register_library_position_tests(TestRunner& runner) {
    runner.add("LibraryPosition: calculateLibraryPosition with small index", [&runner]() -> bool {
        const std::string name = "LibraryPosition: calculateLibraryPosition with small index";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            cpp_int index = 42;
            auto    pos   = calculateLibraryPosition(index);

            // index 42 should be in room 0 (since 42 < 9600)
            // Room 0 should encode as "A" (single zero byte)
            // wall = (42 / 2400) % 4 = 0
            // shelf = (42 / 480) % 5 = 0
            // album = (42 / 15) % 32 = 2
            // track = 42 % 15 = 12
            ok &= RUN_CHECK(runner, name, pos.room == "", "room is \"\" (empty = 0)");
            ok &= RUN_CHECK(runner, name, pos.wall == 0, "wall is 0");
            ok &= RUN_CHECK(runner, name, pos.shelf == 0, "shelf is 0");
            ok &= RUN_CHECK(runner, name, pos.album == 2, "album is 2");
            ok &= RUN_CHECK(runner, name, pos.track == 12, "track is 12");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("LibraryPosition: reconstructIndexFromPosition roundtrip", [&runner]() -> bool {
        const std::string name = "LibraryPosition: reconstructIndexFromPosition roundtrip";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            cpp_int original_index = 12345;
            auto    pos            = calculateLibraryPosition(original_index);
            cpp_int reconstructed  = reconstructIndexFromPosition(pos);

            ok &= RUN_CHECK(runner, name, original_index == reconstructed, "index roundtrip successful");
            // Room 1 should be base64-encoded representation of 1
            ok &= RUN_CHECK(runner, name, !pos.room.empty(), "room is not empty");
            ok &= RUN_CHECK(runner, name, pos.wall == 1, "correct wall");
            ok &= RUN_CHECK(runner, name, pos.shelf == 0, "correct shelf");
            ok &= RUN_CHECK(runner, name, pos.album == 23, "correct album");
            ok &= RUN_CHECK(runner, name, pos.track == 0, "correct track");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("LibraryPosition: position at room boundary", [&runner]() -> bool {
        const std::string name = "LibraryPosition: position at room boundary";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            cpp_int index = 9600; // First index in room 1
            auto    pos   = calculateLibraryPosition(index);

            ok &= RUN_CHECK(runner, name, !pos.room.empty(), "room is not empty (should be room 1)");
            ok &= RUN_CHECK(runner, name, pos.wall == 0, "wall is 0");
            ok &= RUN_CHECK(runner, name, pos.shelf == 0, "shelf is 0");
            ok &= RUN_CHECK(runner, name, pos.album == 0, "album is 0");
            ok &= RUN_CHECK(runner, name, pos.track == 0, "track is 0");

            // Verify roundtrip
            cpp_int reconstructed = reconstructIndexFromPosition(pos);
            ok &= RUN_CHECK(runner, name, index == reconstructed, "boundary roundtrip successful");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("LibraryPosition: large index roundtrip", [&runner]() -> bool {
        const std::string name = "LibraryPosition: large index roundtrip";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            cpp_int index         = 50000;
            auto    pos           = calculateLibraryPosition(index);
            cpp_int reconstructed = reconstructIndexFromPosition(pos);

            ok &= RUN_CHECK(runner, name, index == reconstructed, "large index roundtrip successful");
            ok &= RUN_CHECK(runner, name, !pos.room.empty(), "room is not empty (large room number)");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("LibraryPosition: IndexMetadata includes position field", [&runner]() -> bool {
        const std::string name = "LibraryPosition: IndexMetadata includes position field";
        bool              ok   = true;
        try {
            // Create a simple base64 index
            std::vector<uint8_t> bytes  = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
            std::string          base64 = Utilities::encodeBase64Url(bytes);

            auto meta = IndexMetadata::extractMetadataFromIndex(base64);

            // Check that position was calculated
            ok &= RUN_CHECK(runner, name, !meta.genre.empty(), "genre non-empty");
            ok &= RUN_CHECK(runner, name, !meta.artist.empty(), "artist non-empty");
            ok &= RUN_CHECK(runner, name, !meta.album.empty(), "album non-empty");
            ok &= RUN_CHECK(runner, name, !meta.track.empty(), "track non-empty");
            ok &= RUN_CHECK(runner, name, !meta.position.room.empty(), "position has room field (base64 string)");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("LibraryPosition: cpp_int overload includes position", [&runner]() -> bool {
        const std::string name = "LibraryPosition: cpp_int overload includes position";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            cpp_int index = 123456789;

            auto meta       = IndexMetadata::extractMetadataFromIndex(index);
            auto pos_direct = calculateLibraryPosition(index);

            // Check that both methods produce the same position
            ok &= RUN_CHECK(runner, name, meta.position.room == pos_direct.room, "room matches");
            ok &= RUN_CHECK(runner, name, meta.position.wall == pos_direct.wall, "wall matches");
            ok &= RUN_CHECK(runner, name, meta.position.shelf == pos_direct.shelf, "shelf matches");
            ok &= RUN_CHECK(runner, name, meta.position.album == pos_direct.album, "album matches");
            ok &= RUN_CHECK(runner, name, meta.position.track == pos_direct.track, "track matches");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("LibraryPosition: cross-module consistency with IndexMetadata", [&runner]() -> bool {
        const std::string name = "LibraryPosition: cross-module consistency with IndexMetadata";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            // Test comprehensive list of indexes covering various edge cases
            std::vector<cpp_int> test_indexes = {
                0,                         // Origin
                1,                         // First track
                14,                        // Last track in first album
                15,                        // First track in second album
                479,                       // Last track in first shelf
                480,                       // First track in second shelf
                2399,                      // Last track in first wall
                2400,                      // First track in second wall
                9599,                      // Last track in first room
                9600,                      // First track in second room
                12345,                     // Arbitrary value
                123456789,                 // Large value
                cpp_int("999999999999999") // Very large value
            };

            for (const auto& index : test_indexes) {
                // Extract metadata using IndexMetadata (which internally uses LibraryPosition)
                auto meta = IndexMetadata::extractMetadataFromIndex(index);

                // Calculate position directly using LibraryPosition function
                auto pos_direct = calculateLibraryPosition(index);

                // Verify perfect consistency across all position fields
                bool positions_match = (meta.position.room == pos_direct.room) && (meta.position.wall == pos_direct.wall) &&
                                       (meta.position.shelf == pos_direct.shelf) && (meta.position.album == pos_direct.album) &&
                                       (meta.position.track == pos_direct.track);

                if (!positions_match) {
                    std::ostringstream oss;
                    oss << "position mismatch for index " << index << ": "
                        << "metadata=(" << meta.position.room << "," << (int) meta.position.wall << "," << (int) meta.position.shelf << ","
                        << (int) meta.position.album << "," << (int) meta.position.track << ") vs "
                        << "direct=(" << pos_direct.room << "," << (int) pos_direct.wall << "," << (int) pos_direct.shelf << ","
                        << (int) pos_direct.album << "," << (int) pos_direct.track << ")";
                    runner.failMsg(name, oss.str());
                    ok = false;
                }

                // Additionally verify that reconstructing the index from the position
                // in the metadata gives us back the original index
                cpp_int reconstructed = reconstructIndexFromPosition(meta.position);
                if (reconstructed != index) {
                    std::ostringstream oss;
                    oss << "reconstruction mismatch for index " << index << ": reconstructed as " << reconstructed;
                    runner.failMsg(name, oss.str());
                    ok = false;
                }
            }
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("LibraryPosition: zero index maps to origin", [&runner]() -> bool {
        const std::string name = "LibraryPosition: zero index maps to origin";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            cpp_int index = 0;
            auto    pos   = calculateLibraryPosition(index);

            ok &= RUN_CHECK(runner, name, pos.room == "", "room is \"\" (empty = 0)");
            ok &= RUN_CHECK(runner, name, pos.wall == 0, "wall is 0");
            ok &= RUN_CHECK(runner, name, pos.shelf == 0, "shelf is 0");
            ok &= RUN_CHECK(runner, name, pos.album == 0, "album is 0");
            ok &= RUN_CHECK(runner, name, pos.track == 0, "track is 0");

            // Verify roundtrip
            cpp_int reconstructed = reconstructIndexFromPosition(pos);
            ok &= RUN_CHECK(runner, name, index == reconstructed, "zero roundtrip successful");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("LibraryPosition: constants validation", [&runner]() -> bool {
        const std::string name = "LibraryPosition: constants validation";
        using namespace LibraryConstants;
        bool ok = true;

        ok &= RUN_CHECK(runner, name, TRACKS_PER_ALBUM == 15, "TRACKS_PER_ALBUM == 15");
        ok &= RUN_CHECK(runner, name, ALBUMS_PER_SHELF == 32, "ALBUMS_PER_SHELF == 32");
        ok &= RUN_CHECK(runner, name, SHELVES_PER_WALL == 5, "SHELVES_PER_WALL == 5");
        ok &= RUN_CHECK(runner, name, WALLS_PER_ROOM == 4, "WALLS_PER_ROOM == 4");
        ok &= RUN_CHECK(runner, name, ITEMS_PER_ALBUM == 15, "ITEMS_PER_ALBUM == 15");
        ok &= RUN_CHECK(runner, name, ITEMS_PER_SHELF == 480, "ITEMS_PER_SHELF == 480");
        ok &= RUN_CHECK(runner, name, ITEMS_PER_WALL == 2400, "ITEMS_PER_WALL == 2400");
        ok &= RUN_CHECK(runner, name, ITEMS_PER_ROOM == 9600, "ITEMS_PER_ROOM == 9600");

        return ok;
    });

    runner.add("LibraryPosition: uniqueness - consecutive indexes map to different positions", [&runner]() -> bool {
        const std::string name = "LibraryPosition: uniqueness - consecutive indexes map to different positions";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            // Test that consecutive indexes produce different positions
            for (int i = 0; i < 100; i++) {
                cpp_int idx1 = i;
                cpp_int idx2 = i + 1;

                auto pos1 = calculateLibraryPosition(idx1);
                auto pos2 = calculateLibraryPosition(idx2);

                // At least one field should be different
                bool different = (pos1.room != pos2.room) || (pos1.wall != pos2.wall) || (pos1.shelf != pos2.shelf) || (pos1.album != pos2.album) ||
                                 (pos1.track != pos2.track);

                if (!different) {
                    std::ostringstream oss;
                    oss << "indexes " << i << " and " << (i + 1) << " map to same position";
                    runner.failMsg(name, oss.str());
                    ok = false;
                    break;
                }
            }
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("LibraryPosition: perfect bijection - all positions in range are reachable", [&runner]() -> bool {
        const std::string name = "LibraryPosition: perfect bijection - all positions in range are reachable";
        using boost::multiprecision::cpp_int;
        using namespace LibraryConstants;
        bool ok = true;
        try {
            // Test all positions in first album (room 0, wall 0, shelf 0, album 0)
            for (uint8_t track = 0; track < TRACKS_PER_ALBUM; track++) {
                LibraryPosition pos;
                pos.room  = ""; // Room 0 is empty string
                pos.wall  = 0;
                pos.shelf = 0;
                pos.album = 0;
                pos.track = track;

                cpp_int reconstructed = reconstructIndexFromPosition(pos);

                // Should be 0-14
                ok &= RUN_CHECK(runner, name, reconstructed == track, "track " + std::to_string(track) + " reconstructs correctly");
            }

            // Test first position of each album in first shelf (room 0, wall 0, shelf 0)
            for (uint8_t album = 0; album < ALBUMS_PER_SHELF; album++) {
                LibraryPosition pos;
                pos.room  = ""; // Room 0 is empty string
                pos.wall  = 0;
                pos.shelf = 0;
                pos.album = album;
                pos.track = 0;

                cpp_int reconstructed = reconstructIndexFromPosition(pos);
                cpp_int expected      = album * ITEMS_PER_ALBUM;

                ok &= RUN_CHECK(runner, name, reconstructed == expected, "album " + std::to_string(album) + " starts at correct index");
            }
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("LibraryPosition: boundary values produce correct positions", [&runner]() -> bool {
        const std::string name = "LibraryPosition: boundary values produce correct positions";
        using boost::multiprecision::cpp_int;
        using namespace LibraryConstants;
        bool ok = true;
        try {
            // Last track of first album (index 14)
            {
                cpp_int index = 14;
                auto    pos   = calculateLibraryPosition(index);
                ok &= RUN_CHECK(runner, name, pos.room == "", "last track room \"\" (empty = 0)");
                ok &= RUN_CHECK(runner, name, pos.album == 0, "last track album 0");
                ok &= RUN_CHECK(runner, name, pos.track == 14, "last track is 14");
            }

            // First track of second album (index 15)
            {
                cpp_int index = 15;
                auto    pos   = calculateLibraryPosition(index);
                ok &= RUN_CHECK(runner, name, pos.room == "", "second album room \"\" (empty = 0)");
                ok &= RUN_CHECK(runner, name, pos.album == 1, "second album is 1");
                ok &= RUN_CHECK(runner, name, pos.track == 0, "second album first track is 0");
            }

            // Last track of first shelf (index 479)
            {
                cpp_int index = 479;
                auto    pos   = calculateLibraryPosition(index);
                ok &= RUN_CHECK(runner, name, pos.room == "", "last shelf track room \"\" (empty = 0)");
                ok &= RUN_CHECK(runner, name, pos.shelf == 0, "last shelf track shelf 0");
                ok &= RUN_CHECK(runner, name, pos.album == 31, "last shelf track album 31");
                ok &= RUN_CHECK(runner, name, pos.track == 14, "last shelf track track 14");
            }

            // First track of second shelf (index 480)
            {
                cpp_int index = 480;
                auto    pos   = calculateLibraryPosition(index);
                ok &= RUN_CHECK(runner, name, pos.room == "", "second shelf room \"\" (empty = 0)");
                ok &= RUN_CHECK(runner, name, pos.shelf == 1, "second shelf is 1");
                ok &= RUN_CHECK(runner, name, pos.album == 0, "second shelf album 0");
                ok &= RUN_CHECK(runner, name, pos.track == 0, "second shelf track 0");
            }

            // Last track of first wall (index 2399)
            {
                cpp_int index = 2399;
                auto    pos   = calculateLibraryPosition(index);
                ok &= RUN_CHECK(runner, name, pos.room == "", "last wall track room \"\" (empty = 0)");
                ok &= RUN_CHECK(runner, name, pos.wall == 0, "last wall track wall 0");
                ok &= RUN_CHECK(runner, name, pos.shelf == 4, "last wall track shelf 4");
                ok &= RUN_CHECK(runner, name, pos.album == 31, "last wall track album 31");
                ok &= RUN_CHECK(runner, name, pos.track == 14, "last wall track track 14");
            }

            // First track of second wall (index 2400)
            {
                cpp_int index = 2400;
                auto    pos   = calculateLibraryPosition(index);
                ok &= RUN_CHECK(runner, name, pos.room == "", "second wall room \"\" (empty = 0)");
                ok &= RUN_CHECK(runner, name, pos.wall == 1, "second wall is 1");
                ok &= RUN_CHECK(runner, name, pos.shelf == 0, "second wall shelf 0");
                ok &= RUN_CHECK(runner, name, pos.album == 0, "second wall album 0");
                ok &= RUN_CHECK(runner, name, pos.track == 0, "second wall track 0");
            }

            // Last track of first room (index 9599)
            {
                cpp_int index = 9599;
                auto    pos   = calculateLibraryPosition(index);
                ok &= RUN_CHECK(runner, name, pos.room == "", "last room track room \"\" (empty = 0)");
                ok &= RUN_CHECK(runner, name, pos.wall == 3, "last room track wall 3");
                ok &= RUN_CHECK(runner, name, pos.shelf == 4, "last room track shelf 4");
                ok &= RUN_CHECK(runner, name, pos.album == 31, "last room track album 31");
                ok &= RUN_CHECK(runner, name, pos.track == 14, "last room track track 14");
            }
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("LibraryPosition: complete roundtrip for various patterns", [&runner]() -> bool {
        const std::string name = "LibraryPosition: complete roundtrip for various patterns";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            // Test various interesting indexes
            std::vector<cpp_int> test_indexes = {
                0,
                1,
                14,
                15,
                16, // Album boundaries
                479,
                480,
                481, // Shelf boundaries
                2399,
                2400,
                2401, // Wall boundaries
                9599,
                9600,
                9601, // Room boundaries
                12345,
                67890,                     // Random values
                123456789,                 // Large value
                cpp_int("999999999999999") // Very large value
            };

            for (const auto& original : test_indexes) {
                auto    pos           = calculateLibraryPosition(original);
                cpp_int reconstructed = reconstructIndexFromPosition(pos);

                if (original != reconstructed) {
                    std::ostringstream oss;
                    oss << "roundtrip failed for index " << original << " -> reconstructed as " << reconstructed;
                    runner.failMsg(name, oss.str());
                    ok = false;
                }
            }
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });
}
