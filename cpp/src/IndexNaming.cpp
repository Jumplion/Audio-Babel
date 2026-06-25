#include "../include/IndexNaming.h"

#include <unordered_set>

#include "../include/LibraryPosition.h"
#include "../include/Utilities.h"

namespace AudioBabel::IndexNaming {

namespace {

    // Per-level salts: arbitrary odd 64-bit constants, mixed in first so they
    // influence the whole avalanche rather than just nudging the final bits.
    // Distinct from IndexScramble's constants so the two features' keying
    // can never alias each other.
    constexpr uint64_t GENRE_SALT  = 0xC2B2AE3D27D4EB4FULL;
    constexpr uint64_t ARTIST_SALT = 0x165667B19E3779F9ULL;
    constexpr uint64_t ALBUM_SALT  = 0x27D4EB2F165667C5ULL;
    constexpr uint64_t TRACK_SALT  = 0x9E3779B185EBCA87ULL;

    constexpr size_t MIN_NAME_LENGTH       = 3;
    constexpr size_t MAX_NAME_LENGTH       = 10;
    constexpr int    MAX_COLLISION_RETRIES = 64;

    auto mixRoom(uint64_t state, const std::string& room) -> uint64_t {
        for (unsigned char c : room) {
            AudioBabel::Utilities::mixIn(state, c);
        }
        return state;
    }

    // Derive one slot's candidate name. `nonce` is bumped by the caller on a
    // collision within the same sibling batch, so retries stay deterministic.
    auto deriveName(uint64_t parentSeed, size_t slot, int nonce) -> std::string {
        uint64_t state = parentSeed;
        AudioBabel::Utilities::mixIn(state, static_cast<uint8_t>(slot & 0xFFU));
        AudioBabel::Utilities::mixIn(state, static_cast<uint8_t>((slot >> 8) & 0xFFU));
        AudioBabel::Utilities::mixIn(state, static_cast<uint8_t>(nonce & 0xFF));

        uint64_t lengthBits = AudioBabel::Utilities::splitmix64(state);
        size_t   length     = MIN_NAME_LENGTH + static_cast<size_t>(lengthBits % (MAX_NAME_LENGTH - MIN_NAME_LENGTH + 1));

        std::string name;
        name.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            uint64_t bits = AudioBabel::Utilities::splitmix64(state);
            name.push_back(AudioBabel::Utilities::BASE64_URL_ALPHA[bits % 64]);
        }
        return name;
    }

} // namespace

auto genreSeed(const std::string& room) -> uint64_t {
    return mixRoom(GENRE_SALT, room);
}

auto artistSeed(const std::string& room, uint8_t wall) -> uint64_t {
    uint64_t state = mixRoom(ARTIST_SALT, room);
    AudioBabel::Utilities::mixIn(state, wall);
    return state;
}

auto albumSeed(const std::string& room, uint8_t wall, uint8_t shelf) -> uint64_t {
    uint64_t state = mixRoom(ALBUM_SALT, room);
    AudioBabel::Utilities::mixIn(state, wall);
    AudioBabel::Utilities::mixIn(state, shelf);
    return state;
}

auto trackSeed(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album) -> uint64_t {
    uint64_t state = mixRoom(TRACK_SALT, room);
    AudioBabel::Utilities::mixIn(state, wall);
    AudioBabel::Utilities::mixIn(state, shelf);
    AudioBabel::Utilities::mixIn(state, album);
    return state;
}

auto generateSiblingNames(uint64_t parentSeed, size_t count) -> std::vector<std::string> {
    std::vector<std::string>        names;
    std::unordered_set<std::string> used;
    names.reserve(count);
    used.reserve(count);

    for (size_t slot = 0; slot < count; ++slot) {
        std::string name;
        int         nonce = 0;
        while (true) {
            name = deriveName(parentSeed, slot, nonce);
            if (used.find(name) == used.end()) {
                break;
            }
            if (++nonce > MAX_COLLISION_RETRIES) {
                // Never expected given the [3,10]-char output space versus
                // sibling-group sizes of at most 32, but keep the uniqueness
                // guarantee unconditional rather than probabilistic.
                name = deriveName(parentSeed, slot, 0) + "-" + std::to_string(slot);
                break;
            }
        }
        used.insert(name);
        names.push_back(std::move(name));
    }
    return names;
}

auto genreNames(const std::string& room) -> std::vector<std::string> {
    return generateSiblingNames(genreSeed(room), LibraryConstants::WALLS_PER_ROOM);
}

auto artistNames(const std::string& room, uint8_t wall) -> std::vector<std::string> {
    return generateSiblingNames(artistSeed(room, wall), LibraryConstants::SHELVES_PER_WALL);
}

auto albumNames(const std::string& room, uint8_t wall, uint8_t shelf) -> std::vector<std::string> {
    return generateSiblingNames(albumSeed(room, wall, shelf), LibraryConstants::ALBUMS_PER_SHELF);
}

auto trackNames(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album) -> std::vector<std::string> {
    return generateSiblingNames(trackSeed(room, wall, shelf, album), LibraryConstants::TRACKS_PER_ALBUM);
}

auto genreNameFor(const std::string& room, uint8_t wall) -> std::string {
    return genreNames(room).at(wall);
}

auto artistNameFor(const std::string& room, uint8_t wall, uint8_t shelf) -> std::string {
    return artistNames(room, wall).at(shelf);
}

auto albumNameFor(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album) -> std::string {
    return albumNames(room, wall, shelf).at(album);
}

auto trackNameFor(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album, uint8_t track) -> std::string {
    return trackNames(room, wall, shelf, album).at(track);
}

} // namespace AudioBabel::IndexNaming
