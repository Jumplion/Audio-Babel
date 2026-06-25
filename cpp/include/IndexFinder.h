#ifndef AUDIOBABEL_INDEX_FINDER_H
#define AUDIOBABEL_INDEX_FINDER_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace AudioBabel {

/**
 * @file IndexFinder.h
 * @brief Search for indexes by a genre/artist/album/track name.
 *
 * IndexNaming's *SlotFor functions decode a name into its coordinate in O(1)
 * given the room it lives in — but a bare name never determines the room.
 * That's not a gap to patch: bounded-length names form a finite space while
 * `room` is an unbounded arbitrary-precision integer, so by the pigeonhole
 * principle no naming scheme can make a short name pin down an unbounded
 * room. Finding indexes by name therefore means trying candidate rooms and
 * asking IndexNaming "does this room have a slot with this name?" — a search
 * that is unavoidable in principle, but cheap in practice now that each
 * room's check is a single O(1) decode rather than a sibling enumeration.
 *
 * findByName() performs that search by scanning rooms 0, 1, 2, ... (via their
 * base64 identifiers) until enough matches are found or the scan budget is
 * exhausted. A match at a level shallower than "track" doesn't pin down a
 * single index — e.g. an artist-level match fixes (room, wall, shelf) but
 * leaves album/track free — so IndexMatch reports one representative index
 * (the first slot under that match) alongside how many indexes share it.
 */
enum class NameLevel : uint8_t { Genre, Artist, Album, Track };

struct IndexMatch {
    std::string room;
    uint8_t     wall  = 0;
    uint8_t     shelf = 0;
    uint8_t     album = 0;
    uint8_t     track = 0;

    NameLevel   level = NameLevel::Track;
    std::string matchedName;

    /// One concrete index within this match (deepest unmatched coordinates
    /// default to 0). Always exact and reconstructible via
    /// reconstructIndexFromPosition — never an approximation.
    boost::multiprecision::cpp_int representativeIndex;
    std::string                    representativeIndexBase64;

    /// How many full indexes share this match (1 for a track-level match;
    /// larger for shallower levels, since deeper coordinates are unconstrained).
    uint64_t indexesAtThisMatch = 1;
};

struct FindOptions {
    /// Hierarchy level the query name is checked against.
    NameLevel level = NameLevel::Track;
    /// Stop once this many matches have been found.
    size_t maxResults = 10;
    /// Upper bound on how many rooms to scan before giving up.
    uint64_t maxRoomsToScan = 2'000'000;
};

namespace IndexFinder {

    /**
     * @brief Search rooms in order for a name at the given hierarchy level.
     * @param query Name to look up (exact match against IndexNaming's output).
     * @param options Search level, result cap, and room-scan budget.
     * @return Up to options.maxResults matches, in ascending room order.
     */
    auto findByName(const std::string& query, const FindOptions& options = {}) -> std::vector<IndexMatch>;

} // namespace IndexFinder
} // namespace AudioBabel

#endif // AUDIOBABEL_INDEX_FINDER_H
