/**
 * @file test_index_naming.cpp
 * @brief Tests for IndexNaming's cosmetic name generation.
 *
 * Covers the properties the rework was built for: determinism, pairwise
 * uniqueness within one sibling batch, stability of a name regardless of
 * which descendant is also queried, decorrelation between adjacent slots,
 * and the configured length bounds.
 */

#include <IndexNaming.h>
#include <LibraryPosition.h>

#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>

#include "test_common.h"

using namespace AudioBabel;

namespace {

constexpr size_t MIN_NAME_LENGTH = 3;
constexpr size_t MAX_NAME_LENGTH = 10;

bool valid_b64_chars(const std::string& s) {
    for (char c : s) {
        if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("IndexNaming: determinism across repeated calls", "[naming][determinism]") {
    std::string room = "Qx7";

    REQUIRE(IndexNaming::genreNameFor(room, 2) == IndexNaming::genreNameFor(room, 2));
    REQUIRE(IndexNaming::artistNameFor(room, 2, 3) == IndexNaming::artistNameFor(room, 2, 3));
    REQUIRE(IndexNaming::albumNameFor(room, 2, 3, 17) == IndexNaming::albumNameFor(room, 2, 3, 17));
    REQUIRE(IndexNaming::trackNameFor(room, 2, 3, 17, 9) == IndexNaming::trackNameFor(room, 2, 3, 17, 9));

    REQUIRE(IndexNaming::genreNames(room) == IndexNaming::genreNames(room));
    REQUIRE(IndexNaming::artistNames(room, 2) == IndexNaming::artistNames(room, 2));
    REQUIRE(IndexNaming::albumNames(room, 2, 3) == IndexNaming::albumNames(room, 2, 3));
    REQUIRE(IndexNaming::trackNames(room, 2, 3, 17) == IndexNaming::trackNames(room, 2, 3, 17));
}

TEST_CASE("IndexNaming: single-leaf wrappers match the batch they're drawn from", "[naming][consistency]") {
    std::string room = "ZxQv9";

    auto genres = IndexNaming::genreNames(room);
    for (uint8_t wall = 0; wall < genres.size(); ++wall) {
        REQUIRE(IndexNaming::genreNameFor(room, wall) == genres[wall]);
    }

    auto artists = IndexNaming::artistNames(room, 1);
    for (uint8_t shelf = 0; shelf < artists.size(); ++shelf) {
        REQUIRE(IndexNaming::artistNameFor(room, 1, shelf) == artists[shelf]);
    }

    auto albums = IndexNaming::albumNames(room, 1, 4);
    for (uint8_t album = 0; album < albums.size(); ++album) {
        REQUIRE(IndexNaming::albumNameFor(room, 1, 4, album) == albums[album]);
    }

    auto tracks = IndexNaming::trackNames(room, 1, 4, 10);
    for (uint8_t track = 0; track < tracks.size(); ++track) {
        REQUIRE(IndexNaming::trackNameFor(room, 1, 4, 10, track) == tracks[track]);
    }
}

TEST_CASE("IndexNaming: names are pairwise unique within one sibling batch", "[naming][uniqueness]") {
    std::vector<std::string> rooms = {"", "A", "Qx7", "ThisIsALongerRoomIdentifier1234"};

    for (const auto& room : rooms) {
        INFO("room: \"" << room << "\"");

        auto genres = IndexNaming::genreNames(room);
        REQUIRE(std::set<std::string>(genres.begin(), genres.end()).size() == genres.size());

        for (uint8_t wall = 0; wall < LibraryConstants::WALLS_PER_ROOM; ++wall) {
            auto artists = IndexNaming::artistNames(room, wall);
            REQUIRE(std::set<std::string>(artists.begin(), artists.end()).size() == artists.size());

            auto albums = IndexNaming::albumNames(room, wall, 0);
            REQUIRE(std::set<std::string>(albums.begin(), albums.end()).size() == albums.size());

            auto tracks = IndexNaming::trackNames(room, wall, 0, 0);
            REQUIRE(std::set<std::string>(tracks.begin(), tracks.end()).size() == tracks.size());
        }
    }
}

TEST_CASE("IndexNaming: a name is stable regardless of which descendant is also queried", "[naming][stability]") {
    std::string room  = "Stab1";
    uint8_t     wall  = 0;
    uint8_t     shelf = 2;
    uint8_t     album = 5;

    // The shelf's artist name must not change depending on which album/track
    // underneath it happens to also be queried.
    std::string artistViaAlbum0 = IndexNaming::artistNameFor(room, wall, shelf);
    (void) IndexNaming::albumNameFor(room, wall, shelf, 0);
    (void) IndexNaming::trackNameFor(room, wall, shelf, 0, 0);
    std::string artistViaAlbum7 = IndexNaming::artistNameFor(room, wall, shelf);
    (void) IndexNaming::albumNameFor(room, wall, shelf, 7);
    (void) IndexNaming::trackNameFor(room, wall, shelf, 7, 14);
    REQUIRE(artistViaAlbum0 == artistViaAlbum7);

    // The album's name must not change depending on which track is queried.
    std::string albumViaTrack0  = IndexNaming::albumNameFor(room, wall, shelf, album);
    std::string trackName0      = IndexNaming::trackNameFor(room, wall, shelf, album, 0);
    std::string albumViaTrack14 = IndexNaming::albumNameFor(room, wall, shelf, album);
    std::string trackName14     = IndexNaming::trackNameFor(room, wall, shelf, album, 14);
    REQUIRE(albumViaTrack0 == albumViaTrack14);
    REQUIRE(trackName0 != trackName14);
}

TEST_CASE("IndexNaming: adjacent slots are decorrelated", "[naming][decorrelation]") {
    // Neighbouring slots (differing by exactly 1) must not produce names that
    // share more than a one-character prefix — the failure mode the rework
    // was meant to fix (e.g. "AZBY" next to "AZBX").
    std::string room   = "Decor";
    auto        albums = IndexNaming::albumNames(room, 0, 0);

    int sharedPrefixCount = 0;
    for (size_t i = 0; i + 1 < albums.size(); ++i) {
        const std::string& a            = albums[i];
        const std::string& b            = albums[i + 1];
        size_t             commonPrefix = 0;
        while (commonPrefix < a.size() && commonPrefix < b.size() && a[commonPrefix] == b[commonPrefix]) {
            ++commonPrefix;
        }
        if (commonPrefix > 1) {
            ++sharedPrefixCount;
        }
    }
    // Allow rare coincidental matches, but the overwhelming majority of
    // adjacent pairs must not look related.
    REQUIRE(sharedPrefixCount <= 1);
}

TEST_CASE("IndexNaming: generated names stay within the configured length and alphabet", "[naming][bounds]") {
    std::string room = "Bounds";

    auto checkAll = [](const std::vector<std::string>& names) {
        for (const auto& name : names) {
            REQUIRE(name.length() >= MIN_NAME_LENGTH);
            REQUIRE(name.length() <= MAX_NAME_LENGTH);
            REQUIRE(valid_b64_chars(name));
        }
    };

    checkAll(IndexNaming::genreNames(room));
    checkAll(IndexNaming::artistNames(room, 0));
    checkAll(IndexNaming::albumNames(room, 0, 0));
    checkAll(IndexNaming::trackNames(room, 0, 0, 0));
}

TEST_CASE("IndexNaming: empty room (room 0) generates normal names, not placeholders", "[naming][edge_case]") {
    auto genres = IndexNaming::genreNames("");
    REQUIRE(genres.size() == LibraryConstants::WALLS_PER_ROOM);
    for (const auto& name : genres) {
        REQUIRE(name.length() >= MIN_NAME_LENGTH);
        REQUIRE(valid_b64_chars(name));
    }
}
