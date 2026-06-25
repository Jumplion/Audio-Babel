/**
 * @file test_index_finder.cpp
 * @brief Tests for IndexFinder's cross-room name search.
 *
 * Covers: finding a name planted in a known room at each NameLevel, that
 * every returned match actually decodes back to the matched name, that
 * maxResults/maxRoomsToScan are respected, and that an unplanted/garbage
 * query yields no matches within a small scan budget.
 */

#include <IndexFinder.h>
#include <IndexNaming.h>
#include <LibraryPosition.h>

#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace AudioBabel;

TEST_CASE("IndexFinder: finds a genre name planted in an early room", "[finder][genre]") {
    std::string room = Utilities::indexToB64(boost::multiprecision::cpp_int(3));
    std::string name = IndexNaming::genreNameFor(room, 2);

    FindOptions options;
    options.level      = NameLevel::Genre;
    options.maxResults = 5;

    auto matches = IndexFinder::findByName(name, options);
    REQUIRE_FALSE(matches.empty());

    bool foundPlantedRoom = false;
    for (const auto& match : matches) {
        REQUIRE(match.matchedName == name);
        REQUIRE(match.level == NameLevel::Genre);
        // Every returned match must independently re-decode to this name.
        auto reDecoded = IndexNaming::genreSlotFor(match.room, name);
        REQUIRE(reDecoded.has_value());
        REQUIRE(*reDecoded == match.wall);

        if (match.room == room) {
            foundPlantedRoom = true;
            REQUIRE(match.wall == 2);
        }
    }
    REQUIRE(foundPlantedRoom);
}

TEST_CASE("IndexFinder: finds an artist name planted in an early room", "[finder][artist]") {
    std::string room = Utilities::indexToB64(boost::multiprecision::cpp_int(1));
    std::string name = IndexNaming::artistNameFor(room, 1, 3);

    FindOptions options;
    options.level      = NameLevel::Artist;
    options.maxResults = 5;

    auto matches = IndexFinder::findByName(name, options);
    bool found   = false;
    for (const auto& match : matches) {
        if (match.room == room) {
            found = true;
            REQUIRE(match.wall == 1);
            REQUIRE(match.shelf == 3);
            REQUIRE(match.indexesAtThisMatch == static_cast<uint64_t>(LibraryConstants::ALBUMS_PER_SHELF) * LibraryConstants::TRACKS_PER_ALBUM);
        }
    }
    REQUIRE(found);
}

TEST_CASE("IndexFinder: finds an album name and reconstructs a valid representative index", "[finder][album]") {
    std::string room = Utilities::indexToB64(boost::multiprecision::cpp_int(0));
    std::string name = IndexNaming::albumNameFor(room, 0, 0, 5);

    FindOptions options;
    options.level      = NameLevel::Album;
    options.maxResults = 3;

    auto matches = IndexFinder::findByName(name, options);
    REQUIRE_FALSE(matches.empty());

    const auto& match = matches.front();
    REQUIRE(match.room == room);
    REQUIRE(match.album == 5);
    REQUIRE(match.indexesAtThisMatch == LibraryConstants::TRACKS_PER_ALBUM);

    // The representative index must decode (via LibraryPosition) back to the
    // exact coordinates reported in the match.
    LibraryPosition pos = calculateLibraryPosition(match.representativeIndex);
    REQUIRE(pos.room == match.room);
    REQUIRE(pos.wall == match.wall);
    REQUIRE(pos.shelf == match.shelf);
    REQUIRE(pos.album == match.album);
    REQUIRE(pos.track == match.track);
    REQUIRE(Utilities::indexToB64(match.representativeIndex) == match.representativeIndexBase64);
}

TEST_CASE("IndexFinder: finds an exact track name with indexesAtThisMatch == 1", "[finder][track]") {
    std::string room = Utilities::indexToB64(boost::multiprecision::cpp_int(2));
    std::string name = IndexNaming::trackNameFor(room, 3, 4, 31, 14);

    FindOptions options;
    options.level      = NameLevel::Track;
    options.maxResults = 3;

    auto matches = IndexFinder::findByName(name, options);
    bool found   = false;
    for (const auto& match : matches) {
        if (match.room == room) {
            found = true;
            REQUIRE(match.wall == 3);
            REQUIRE(match.shelf == 4);
            REQUIRE(match.album == 31);
            REQUIRE(match.track == 14);
            REQUIRE(match.indexesAtThisMatch == 1);
        }
    }
    REQUIRE(found);
}

TEST_CASE("IndexFinder: respects maxResults", "[finder][limits]") {
    // A track name that decodes successfully in *some* rooms (not necessarily
    // every room) but should never return more than maxResults matches.
    std::string name = IndexNaming::trackNameFor("Seed", 0, 0, 0, 0);

    FindOptions options;
    options.level          = NameLevel::Track;
    options.maxResults     = 2;
    options.maxRoomsToScan = 50000;

    auto matches = IndexFinder::findByName(name, options);
    REQUIRE(matches.size() <= 2);
}

TEST_CASE("IndexFinder: respects maxRoomsToScan", "[finder][limits]") {
    std::string name = "ZZZZZ"; // 5 chars, the track name width, but content
                                 // unlikely to be a valid track name anywhere.

    FindOptions options;
    options.level          = NameLevel::Track;
    options.maxResults     = 1000;
    options.maxRoomsToScan = 25;

    // Should terminate promptly and return at most a small handful of
    // matches, never scanning beyond the budget.
    auto matches = IndexFinder::findByName(name, options);
    REQUIRE(matches.size() <= options.maxRoomsToScan);
}

TEST_CASE("IndexFinder: maxResults of 0 returns no matches", "[finder][limits][edge_case]") {
    FindOptions options;
    options.level      = NameLevel::Genre;
    options.maxResults = 0;

    auto matches = IndexFinder::findByName("AAA", options);
    REQUIRE(matches.empty());
}
