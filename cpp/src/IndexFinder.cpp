#include "../include/IndexFinder.h"

#include "../include/IndexNaming.h"
#include "../include/LibraryPosition.h"
#include "../include/Utilities.h"

namespace AudioBabel::IndexFinder {

namespace {

    auto buildMatch(const std::string& room, NameLevel level, const std::string& query, uint8_t wall, uint8_t shelf, uint8_t album,
                     uint8_t track, uint64_t indexesAtThisMatch) -> IndexMatch {
        IndexMatch match;
        match.room               = room;
        match.wall               = wall;
        match.shelf              = shelf;
        match.album              = album;
        match.track              = track;
        match.level              = level;
        match.matchedName        = query;
        match.indexesAtThisMatch = indexesAtThisMatch;

        LibraryPosition position{room, wall, shelf, album, track};
        match.representativeIndex       = reconstructIndexFromPosition(position);
        match.representativeIndexBase64 = Utilities::indexToB64(match.representativeIndex);
        return match;
    }

    // Try matching `query` against `room` at `level`. Returns true and fills
    // `match` on success.
    auto tryRoom(const std::string& room, NameLevel level, const std::string& query, IndexMatch& match) -> bool {
        switch (level) {
            case NameLevel::Genre: {
                auto wall = IndexNaming::genreSlotFor(room, query);
                if (!wall) {
                    return false;
                }
                uint64_t count = static_cast<uint64_t>(LibraryConstants::SHELVES_PER_WALL) * LibraryConstants::ALBUMS_PER_SHELF *
                                  LibraryConstants::TRACKS_PER_ALBUM;
                match = buildMatch(room, level, query, *wall, 0, 0, 0, count);
                return true;
            }
            case NameLevel::Artist: {
                auto slot = IndexNaming::artistSlotFor(room, query);
                if (!slot) {
                    return false;
                }
                uint64_t count = static_cast<uint64_t>(LibraryConstants::ALBUMS_PER_SHELF) * LibraryConstants::TRACKS_PER_ALBUM;
                match = buildMatch(room, level, query, slot->wall, slot->shelf, 0, 0, count);
                return true;
            }
            case NameLevel::Album: {
                auto slot = IndexNaming::albumSlotFor(room, query);
                if (!slot) {
                    return false;
                }
                match = buildMatch(room, level, query, slot->wall, slot->shelf, slot->album, 0, LibraryConstants::TRACKS_PER_ALBUM);
                return true;
            }
            case NameLevel::Track: {
                auto slot = IndexNaming::trackSlotFor(room, query);
                if (!slot) {
                    return false;
                }
                match = buildMatch(room, level, query, slot->wall, slot->shelf, slot->album, slot->track, 1);
                return true;
            }
        }
        return false;
    }

} // namespace

auto findByName(const std::string& query, const FindOptions& options) -> std::vector<IndexMatch> {
    std::vector<IndexMatch> results;
    if (options.maxResults == 0) {
        return results;
    }

    for (uint64_t roomNum = 0; roomNum < options.maxRoomsToScan; ++roomNum) {
        std::string room = Utilities::indexToB64(boost::multiprecision::cpp_int(roomNum));

        IndexMatch match;
        if (tryRoom(room, options.level, query, match)) {
            results.push_back(std::move(match));
            if (results.size() >= options.maxResults) {
                break;
            }
        }
    }

    return results;
}

} // namespace AudioBabel::IndexFinder
