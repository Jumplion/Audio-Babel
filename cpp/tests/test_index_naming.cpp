/**
 * @file test_index_naming.cpp
 * @brief Tests for IndexNaming's byte-derived metadata names.
 *
 * Covers the properties the redesign was built for: names are a deterministic
 * function of the index, neighbouring indexes get wildly different names,
 * desired names can be inverted straight into indexes that carry them (no
 * search), partial queries leave free fields varied, and names stay within the
 * configured 1..NAME_MAX_CHARS width.
 */

#include <IndexNaming.h>
#include <LibraryPosition.h>
#include <Utilities.h>

#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>

using namespace AudioBabel;
using boost::multiprecision::cpp_int;

namespace {

bool valid_b64_chars(const std::string& s) {
    for (char c : s) {
        if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

void requireWellFormed(const IndexNaming::Names& n) {
    for (const std::string* s : {&n.genre, &n.artist, &n.album, &n.track}) {
        REQUIRE_FALSE(s->empty());
        REQUIRE(s->size() <= IndexNaming::nameMaxChars());
        REQUIRE(valid_b64_chars(*s));
    }
}

} // namespace

TEST_CASE("IndexNaming: names are a deterministic function of the index", "[naming][determinism]") {
    std::vector<cpp_int> samples = {cpp_int(0), cpp_int(1), cpp_int(123456789), cpp_int("340282366920938463463374607431768211455")};
    for (const auto& idx : samples) {
        auto a = IndexNaming::namesForIndex(idx);
        auto b = IndexNaming::namesForIndex(idx);
        REQUIRE(a.genre == b.genre);
        REQUIRE(a.artist == b.artist);
        REQUIRE(a.album == b.album);
        REQUIRE(a.track == b.track);
        requireWellFormed(a);
    }
}

TEST_CASE("IndexNaming: neighbouring indexes get wildly different names", "[naming][decorrelation]") {
    // Adjacent indexes (differing by 1) should rarely share even a single
    // field name — the avalanche from the keyed permutation scatters them.
    int collisions = 0;
    for (int i = 0; i < 1000; ++i) {
        auto a = IndexNaming::namesForIndex(cpp_int(i));
        auto b = IndexNaming::namesForIndex(cpp_int(i + 1));
        if (a.genre == b.genre || a.artist == b.artist || a.album == b.album || a.track == b.track) {
            ++collisions;
        }
    }
    // With four ~14-decimal-digit field spaces, coincidental collisions are
    // astronomically unlikely; allow a tiny margin all the same.
    REQUIRE(collisions <= 2);
}

TEST_CASE("IndexNaming: a desired set of names inverts straight into indexes carrying them", "[naming][construct]") {
    auto target = IndexNaming::namesForIndex(cpp_int("99887766554433221100"));

    IndexNaming::NameQuery q;
    q.genre  = target.genre;
    q.artist = target.artist;
    q.album  = target.album;
    q.track  = target.track;

    auto indexes = IndexNaming::constructIndexesForNames(q, 10, /*seed=*/42);
    REQUIRE(indexes.size() == 10);

    std::set<std::string> distinct;
    for (const auto& idx : indexes) {
        auto names = IndexNaming::namesForIndex(idx);
        REQUIRE(names.genre == target.genre);
        REQUIRE(names.artist == target.artist);
        REQUIRE(names.album == target.album);
        REQUIRE(names.track == target.track);
        distinct.insert(Utilities::indexToB64(idx));
    }
    // Same names, different indexes (the high "discriminator" bits differ).
    REQUIRE(distinct.size() == indexes.size());
}

TEST_CASE("IndexNaming: construction is reproducible for a given seed", "[naming][construct][determinism]") {
    IndexNaming::NameQuery q;
    q.artist = IndexNaming::namesForIndex(cpp_int(555)).artist;

    auto a = IndexNaming::constructIndexesForNames(q, 5, /*seed=*/7);
    auto b = IndexNaming::constructIndexesForNames(q, 5, /*seed=*/7);
    REQUIRE(a == b);
}

TEST_CASE("IndexNaming: a partial query pins the named field and varies the rest", "[naming][construct][partial]") {
    auto target = IndexNaming::namesForIndex(cpp_int(31415926535));

    IndexNaming::NameQuery q;
    q.genre = target.genre; // only genre pinned

    auto indexes = IndexNaming::constructIndexesForNames(q, 12, /*seed=*/2024);
    REQUIRE_FALSE(indexes.empty());

    std::set<std::string> artists;
    for (const auto& idx : indexes) {
        auto names = IndexNaming::namesForIndex(idx);
        REQUIRE(names.genre == target.genre);
        artists.insert(names.artist);
    }
    REQUIRE(artists.size() >= 2); // free fields are genuinely varied
}

TEST_CASE("IndexNaming: an unproducible name yields no indexes", "[naming][construct][negative]") {
    IndexNaming::NameQuery tooLong;
    tooLong.genre = std::string(IndexNaming::nameMaxChars() + 1, 'A');
    REQUIRE(IndexNaming::constructIndexesForNames(tooLong, 5, 1).empty());

    IndexNaming::NameQuery badChar;
    badChar.album = std::string("ab@d");
    REQUIRE(IndexNaming::constructIndexesForNames(badChar, 5, 1).empty());

    IndexNaming::NameQuery empty; // nothing pinned at all
    REQUIRE_FALSE(IndexNaming::constructIndexesForNames(empty, 3, 1).empty());
}

TEST_CASE("IndexNaming: round-trips every producible field name", "[naming][construct][roundtrip]") {
    // Pin each field in turn to a known name and confirm it comes back exactly.
    auto target = IndexNaming::namesForIndex(cpp_int(8675309));

    auto pinAndCheck = [&](auto setter, auto getter) {
        IndexNaming::NameQuery q;
        setter(q);
        auto indexes = IndexNaming::constructIndexesForNames(q, 3, 11);
        REQUIRE_FALSE(indexes.empty());
        for (const auto& idx : indexes) {
            REQUIRE(getter(IndexNaming::namesForIndex(idx)));
        }
    };

    pinAndCheck([&](IndexNaming::NameQuery& q) { q.genre = target.genre; }, [&](const IndexNaming::Names& n) { return n.genre == target.genre; });
    pinAndCheck([&](IndexNaming::NameQuery& q) { q.artist = target.artist; }, [&](const IndexNaming::Names& n) { return n.artist == target.artist; });
    pinAndCheck([&](IndexNaming::NameQuery& q) { q.album = target.album; }, [&](const IndexNaming::Names& n) { return n.album == target.album; });
    pinAndCheck([&](IndexNaming::NameQuery& q) { q.track = target.track; }, [&](const IndexNaming::Names& n) { return n.track == target.track; });
}

TEST_CASE("IndexNaming: batch accessors return per-slot names of the right width and count", "[naming][browse]") {
    std::vector<std::string> rooms = {"", "A", "Qx7", "ThisIsALongerRoomIdentifier1234"};
    for (const auto& room : rooms) {
        INFO("room: \"" << room << "\"");

        auto genres = IndexNaming::genreNames(room);
        REQUIRE(genres.size() == LibraryConstants::WALLS_PER_ROOM);

        auto artists = IndexNaming::artistNames(room, 1);
        REQUIRE(artists.size() == LibraryConstants::SHELVES_PER_WALL);

        auto albums = IndexNaming::albumNames(room, 1, 2);
        REQUIRE(albums.size() == LibraryConstants::ALBUMS_PER_SHELF);

        auto tracks = IndexNaming::trackNames(room, 1, 2, 3);
        REQUIRE(tracks.size() == LibraryConstants::TRACKS_PER_ALBUM);

        for (const auto* batch : {&genres, &artists, &albums, &tracks}) {
            for (const auto& name : *batch) {
                REQUIRE_FALSE(name.empty());
                REQUIRE(name.size() <= IndexNaming::nameMaxChars());
                REQUIRE(valid_b64_chars(name));
            }
        }
    }
}

TEST_CASE("IndexNaming: a browse slot's name matches that index's own metadata", "[naming][browse][consistency]") {
    // The representative-index name a batch accessor shows for a track slot must
    // equal the name namesForIndex gives for that exact reconstructed index.
    std::string room   = "Consist";
    auto        tracks = IndexNaming::trackNames(room, 2, 3, 4);
    for (uint8_t t = 0; t < tracks.size(); ++t) {
        cpp_int idx = reconstructIndexFromPosition(LibraryPosition{room, 2, 3, 4, t});
        REQUIRE(IndexNaming::namesForIndex(idx).track == tracks[t]);
    }
}
