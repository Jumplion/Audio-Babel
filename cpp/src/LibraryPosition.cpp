#include "../include/LibraryPosition.h"

#include "../include/Utilities.h"

namespace {

// Salt for the within-room offset permutation. Distinct from IndexNaming's
// per-level salts and IndexScramble's seed so keying can never alias.
constexpr uint64_t POSITION_SALT  = 0x6C62272E07BB0142ULL;
constexpr int      FEISTEL_ROUNDS = 4;

auto mixRoom(uint64_t state, const std::string& room) -> uint64_t {
    for (unsigned char c : room) {
        AudioBabel::Utilities::mixIn(state, c);
    }
    return state;
}

// Room-keyed seed: room "" (room 0) gets exactly POSITION_SALT; other rooms
// get a fully avalanched variant, giving per-room independent permutations.
auto roomKey(const std::string& room) -> uint64_t {
    return mixRoom(POSITION_SALT, room);
}

auto roundKey(uint64_t key, size_t halfBits, int round) -> uint64_t {
    uint64_t state =
        key ^ (0x9E3779B97F4A7C15ULL * (static_cast<uint64_t>(halfBits) + 1)) ^ (0xD1B54A32D192ED03ULL * (static_cast<uint64_t>(round) + 1));
    return AudioBabel::Utilities::splitmix64(state);
}

auto roundFunction(uint64_t half, size_t halfBits, uint64_t key) -> uint64_t {
    uint64_t state = key;
    AudioBabel::Utilities::mixIn(state, static_cast<uint8_t>(half & 0xFFU));
    AudioBabel::Utilities::mixIn(state, static_cast<uint8_t>((half >> 8) & 0xFFU));
    AudioBabel::Utilities::mixIn(state, static_cast<uint8_t>((half >> 16) & 0xFFU));
    uint64_t mixed = AudioBabel::Utilities::splitmix64(state);
    uint64_t mask  = (halfBits >= 64) ? ~uint64_t(0) : ((uint64_t(1) << halfBits) - 1);
    return mixed & mask;
}

// Smallest even e with 2^e >= n. For n=9600: e=14 (2^14=16384).
auto evenBitsCovering(uint64_t n) -> size_t {
    size_t bits = 0;
    while ((uint64_t(1) << bits) < n) {
        ++bits;
    }
    if (bits % 2 != 0) {
        ++bits;
    }
    return bits;
}

// Balanced Feistel permutation over [0, 2^bits) (bits even).
// Inverted by running rounds in reverse, same as IndexNaming::feistel.
auto feistel(uint64_t x, size_t bits, uint64_t key, bool encrypt) -> uint64_t {
    const size_t   h    = bits / 2;
    const uint64_t mask = (uint64_t(1) << h) - 1;
    uint64_t       hi   = (x >> h) & mask;
    uint64_t       lo   = x & mask;

    if (encrypt) {
        for (int r = 0; r < FEISTEL_ROUNDS; ++r) {
            uint64_t f = roundFunction(lo, h, roundKey(key, h, r));
            uint64_t t = hi ^ f;
            hi         = lo;
            lo         = t;
        }
    } else {
        for (int r = FEISTEL_ROUNDS - 1; r >= 0; --r) {
            uint64_t f = roundFunction(hi, h, roundKey(key, h, r));
            uint64_t t = lo ^ f;
            lo         = hi;
            hi         = t;
        }
    }
    return (hi << h) | lo;
}

// Keyed bijection on [0, domain) via Feistel + cycle-walking
// (Black & Rogaway, "Ciphers with Arbitrary Finite Domains", CT-RSA 2002).
// For domain=9600 and e=14, ~58.6% of inputs land in-range immediately;
// expected ~1.7 Feistel calls per invoke.
auto permuteOffset(uint64_t x, uint64_t domain, uint64_t key) -> uint64_t {
    size_t   e = evenBitsCovering(domain);
    uint64_t y = feistel(x, e, key, /*encrypt=*/true);
    while (y >= domain) {
        y = feistel(y, e, key, /*encrypt=*/true);
    }
    return y;
}

auto unpermuteOffset(uint64_t y, uint64_t domain, uint64_t key) -> uint64_t {
    size_t   e = evenBitsCovering(domain);
    uint64_t x = feistel(y, e, key, /*encrypt=*/false);
    while (x >= domain) {
        x = feistel(x, e, key, /*encrypt=*/false);
    }
    return x;
}

} // anonymous namespace

namespace AudioBabel {

using namespace LibraryConstants;

auto calculateLibraryPosition(const cpp_int& index) -> LibraryPosition {
    LibraryPosition pos;

    cpp_int roomNumber = index / ITEMS_PER_ROOM;
    cpp_int withinRoom = index % ITEMS_PER_ROOM;

    pos.room = Utilities::indexToB64(roomNumber);

    // Unscramble the within-room offset to recover the linear hierarchy position,
    // then decompose into wall/shelf/album/track. Safe cast: withinRoom < 9600.
    uint64_t key          = roomKey(pos.room);
    uint64_t linearOffset = unpermuteOffset(static_cast<uint64_t>(withinRoom), ITEMS_PER_ROOM, key);

    pos.wall           = static_cast<uint8_t>(linearOffset / ITEMS_PER_WALL);
    uint64_t remainder = linearOffset % ITEMS_PER_WALL;
    pos.shelf          = static_cast<uint8_t>(remainder / ITEMS_PER_SHELF);
    remainder          = remainder % ITEMS_PER_SHELF;
    pos.album          = static_cast<uint8_t>(remainder / ITEMS_PER_ALBUM);
    pos.track          = static_cast<uint8_t>(remainder % TRACKS_PER_ALBUM);

    return pos;
}

auto reconstructIndexFromPosition(const LibraryPosition& pos) -> cpp_int {
    // Validate position fields are within documented ranges
    if (pos.wall >= LibraryConstants::WALLS_PER_ROOM) {
        throw std::invalid_argument("wall out of range: " + std::to_string(pos.wall) + " (max " +
                                    std::to_string(LibraryConstants::WALLS_PER_ROOM - 1) + ")");
    }
    if (pos.shelf >= LibraryConstants::SHELVES_PER_WALL) {
        throw std::invalid_argument("shelf out of range: " + std::to_string(pos.shelf) + " (max " +
                                    std::to_string(LibraryConstants::SHELVES_PER_WALL - 1) + ")");
    }
    if (pos.album >= LibraryConstants::ALBUMS_PER_SHELF) {
        throw std::invalid_argument("album out of range: " + std::to_string(pos.album) + " (max " +
                                    std::to_string(LibraryConstants::ALBUMS_PER_SHELF - 1) + ")");
    }
    if (pos.track >= LibraryConstants::TRACKS_PER_ALBUM) {
        throw std::invalid_argument("track out of range: " + std::to_string(pos.track) + " (max " +
                                    std::to_string(LibraryConstants::TRACKS_PER_ALBUM - 1) + ")");
    }

    cpp_int roomNumber = Utilities::b64ToIndex(pos.room);

    // Compute the linear within-room offset, then scramble it so adjacent
    // positions map to well-separated indices (and thus dissimilar audio).
    uint64_t linearOffset = static_cast<uint64_t>(pos.wall) * ITEMS_PER_WALL + static_cast<uint64_t>(pos.shelf) * ITEMS_PER_SHELF +
                            static_cast<uint64_t>(pos.album) * ITEMS_PER_ALBUM + static_cast<uint64_t>(pos.track);

    uint64_t key             = roomKey(pos.room);
    uint64_t scrambledOffset = permuteOffset(linearOffset, ITEMS_PER_ROOM, key);

    return roomNumber * cpp_int(ITEMS_PER_ROOM) + cpp_int(scrambledOffset);
}

} // namespace AudioBabel
