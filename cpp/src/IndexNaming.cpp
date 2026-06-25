#include "../include/IndexNaming.h"

#include <optional>

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

    // Number of Feistel rounds. Four rounds give full avalanche, matching
    // IndexScramble's choice for the same reason.
    constexpr int FEISTEL_ROUNDS = 4;

    // Every real coordinate is multiplied by this before permuting, so the
    // Feistel domain is wider than the real coordinate count. This buys two
    // things: (1) avalanche spreads across the *whole* displayed name instead
    // of leaving high-order, low-entropy zero characters (the real coordinate
    // counts here are all under 10,000, far short of filling a fixed-width
    // base64 field on their own), and (2) a free validity check on decode —
    // a name is only "real" if its decoded value is an exact multiple of this
    // factor, so typos and foreign-room names are rejected without a
    // separate checksum field.
    constexpr uint64_t DECORATION_FACTOR = 4096;

    auto mixRoom(uint64_t state, const std::string& room) -> uint64_t {
        for (unsigned char c : room) {
            AudioBabel::Utilities::mixIn(state, c);
        }
        return state;
    }

    // Room-only key for one naming level. Deliberately independent of any
    // ancestor coordinate (wall/shelf/album) — the coordinate itself is folded
    // into the permuted *value*, not the key. That is what lets a caller
    // decode a name into its full coordinate knowing only (room, name), with
    // no need to already know the ancestors.
    auto levelKey(uint64_t salt, const std::string& room) -> uint64_t {
        return mixRoom(salt, room);
    }

    // Per-round key derived from the level key, half-width and round index.
    auto roundKey(uint64_t key, size_t halfBits, int round) -> uint64_t {
        uint64_t state =
            key ^ (0x9E3779B97F4A7C15ULL * (static_cast<uint64_t>(halfBits) + 1)) ^ (0xD1B54A32D192ED03ULL * (static_cast<uint64_t>(round) + 1));
        return AudioBabel::Utilities::splitmix64(state);
    }

    // Keyed diffusing round function: half-value -> half-value of the same
    // bit width. All half-widths used here are well under 32 bits, so a
    // straightforward byte-wise mixIn chain followed by one splitmix64 step
    // gives ample avalanche; the Feistel structure (not this function) is
    // what provides invertibility.
    auto roundFunction(uint64_t half, size_t halfBits, uint64_t key) -> uint64_t {
        uint64_t state = key;
        AudioBabel::Utilities::mixIn(state, static_cast<uint8_t>(half & 0xFFU));
        AudioBabel::Utilities::mixIn(state, static_cast<uint8_t>((half >> 8) & 0xFFU));
        AudioBabel::Utilities::mixIn(state, static_cast<uint8_t>((half >> 16) & 0xFFU));
        uint64_t mixed = AudioBabel::Utilities::splitmix64(state);
        uint64_t mask  = (halfBits >= 64) ? ~uint64_t(0) : ((uint64_t(1) << halfBits) - 1);
        return mixed & mask;
    }

    // Balanced Feistel permutation over [0, 2^bits) (bits even). Inverted by
    // running the rounds in reverse, exactly like IndexScramble::feistelPow2.
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

    // Smallest even e with 2^e >= n (n >= 1), so the Feistel splits into two
    // equal halves and cycle-walking stays under ~4x expansion.
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

    // Keyed bijection on [0, domain) built from feistel() + cycle-walking
    // (Black & Rogaway, "Ciphers with Arbitrary Finite Domains", CT-RSA 2002):
    // re-apply the power-of-two permutation until the result lands back in
    // [0, domain), exactly as IndexScramble::scramble()/unscramble() do.
    auto permuteEncode(uint64_t x, uint64_t domain, uint64_t key) -> uint64_t {
        size_t   e = evenBitsCovering(domain);
        uint64_t y = feistel(x, e, key, /*encrypt=*/true);
        while (y >= domain) {
            y = feistel(y, e, key, /*encrypt=*/true);
        }
        return y;
    }

    auto permuteDecode(uint64_t y, uint64_t domain, uint64_t key) -> uint64_t {
        size_t   e = evenBitsCovering(domain);
        uint64_t x = feistel(y, e, key, /*encrypt=*/false);
        while (x >= domain) {
            x = feistel(x, e, key, /*encrypt=*/false);
        }
        return x;
    }

    // Smallest width w such that 64^w >= n (n >= 1) — the fixed display width
    // for a level whose expanded domain is n. This is a plain positional
    // base64 (unlike Utilities::indexToB64's bijective numeration), since
    // every name at a given level must be the same length.
    auto digitsCovering(uint64_t n) -> size_t {
        size_t   w   = 0;
        uint64_t cap = 1;
        while (cap < n) {
            cap *= 64;
            ++w;
        }
        return w;
    }

    auto toFixedB64(uint64_t value, size_t width) -> std::string {
        std::string out(width, AudioBabel::Utilities::BASE64_URL_ALPHA[0]);
        for (size_t i = width; i-- > 0;) {
            out[i] = AudioBabel::Utilities::BASE64_URL_ALPHA[value % 64];
            value /= 64;
        }
        return out;
    }

    auto fromFixedB64(const std::string& s, size_t width) -> std::optional<uint64_t> {
        if (s.size() != width) {
            return std::nullopt;
        }
        uint64_t value = 0;
        for (char c : s) {
            int v = AudioBabel::Utilities::base64UrlValue(c);
            if (v < 0) {
                return std::nullopt;
            }
            value = value * 64 + static_cast<uint64_t>(v);
        }
        return value;
    }

    // One hierarchy level's shape: how many real (undecorated) coordinate
    // values it spans, and its salt. realCount always spans every ancestor
    // coordinate this level's name is allowed to vary over (e.g. the artist
    // level spans wall*shelf, not just shelf), so a name alone determines the
    // FULL within-room coordinate down to that level — no separate ancestor
    // names need to be supplied first.
    struct Level {
        uint64_t realCount;
        uint64_t salt;
    };

    auto genreLevel() -> Level {
        return {LibraryConstants::WALLS_PER_ROOM, GENRE_SALT};
    }

    auto artistLevel() -> Level {
        return {static_cast<uint64_t>(LibraryConstants::WALLS_PER_ROOM) * LibraryConstants::SHELVES_PER_WALL, ARTIST_SALT};
    }

    auto albumLevel() -> Level {
        return {static_cast<uint64_t>(LibraryConstants::WALLS_PER_ROOM) * LibraryConstants::SHELVES_PER_WALL * LibraryConstants::ALBUMS_PER_SHELF,
                ALBUM_SALT};
    }

    auto trackLevel() -> Level {
        return {static_cast<uint64_t>(LibraryConstants::WALLS_PER_ROOM) * LibraryConstants::SHELVES_PER_WALL * LibraryConstants::ALBUMS_PER_SHELF *
                    LibraryConstants::TRACKS_PER_ALBUM,
                TRACK_SALT};
    }

    auto encodeSlot(const std::string& room, uint64_t realKey, const Level& level) -> std::string {
        uint64_t domain    = level.realCount * DECORATION_FACTOR;
        uint64_t key       = levelKey(level.salt, room);
        uint64_t expanded  = realKey * DECORATION_FACTOR;
        uint64_t displayed = permuteEncode(expanded, domain, key);
        return toFixedB64(displayed, digitsCovering(domain));
    }

    auto decodeSlot(const std::string& room, const std::string& name, const Level& level) -> std::optional<uint64_t> {
        uint64_t domain = level.realCount * DECORATION_FACTOR;
        auto     parsed = fromFixedB64(name, digitsCovering(domain));
        if (!parsed || *parsed >= domain) {
            return std::nullopt;
        }

        uint64_t key      = levelKey(level.salt, room);
        uint64_t expanded = permuteDecode(*parsed, domain, key);
        if (expanded % DECORATION_FACTOR != 0) {
            return std::nullopt;
        }

        uint64_t realKey = expanded / DECORATION_FACTOR;
        if (realKey >= level.realCount) {
            return std::nullopt;
        }
        return realKey;
    }

} // namespace

auto genreNameFor(const std::string& room, uint8_t wall) -> std::string {
    return encodeSlot(room, wall, genreLevel());
}

auto artistNameFor(const std::string& room, uint8_t wall, uint8_t shelf) -> std::string {
    uint64_t key = static_cast<uint64_t>(wall) * LibraryConstants::SHELVES_PER_WALL + shelf;
    return encodeSlot(room, key, artistLevel());
}

auto albumNameFor(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album) -> std::string {
    uint64_t key = (static_cast<uint64_t>(wall) * LibraryConstants::SHELVES_PER_WALL + shelf) * LibraryConstants::ALBUMS_PER_SHELF + album;
    return encodeSlot(room, key, albumLevel());
}

auto trackNameFor(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album, uint8_t track) -> std::string {
    uint64_t key = ((static_cast<uint64_t>(wall) * LibraryConstants::SHELVES_PER_WALL + shelf) * LibraryConstants::ALBUMS_PER_SHELF + album) *
                       LibraryConstants::TRACKS_PER_ALBUM +
                   track;
    return encodeSlot(room, key, trackLevel());
}

auto genreNames(const std::string& room) -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(LibraryConstants::WALLS_PER_ROOM);
    for (uint8_t wall = 0; wall < LibraryConstants::WALLS_PER_ROOM; ++wall) {
        names.push_back(genreNameFor(room, wall));
    }
    return names;
}

auto artistNames(const std::string& room, uint8_t wall) -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(LibraryConstants::SHELVES_PER_WALL);
    for (uint8_t shelf = 0; shelf < LibraryConstants::SHELVES_PER_WALL; ++shelf) {
        names.push_back(artistNameFor(room, wall, shelf));
    }
    return names;
}

auto albumNames(const std::string& room, uint8_t wall, uint8_t shelf) -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(LibraryConstants::ALBUMS_PER_SHELF);
    for (uint8_t album = 0; album < LibraryConstants::ALBUMS_PER_SHELF; ++album) {
        names.push_back(albumNameFor(room, wall, shelf, album));
    }
    return names;
}

auto trackNames(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album) -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(LibraryConstants::TRACKS_PER_ALBUM);
    for (uint8_t track = 0; track < LibraryConstants::TRACKS_PER_ALBUM; ++track) {
        names.push_back(trackNameFor(room, wall, shelf, album, track));
    }
    return names;
}

auto genreSlotFor(const std::string& room, const std::string& name) -> std::optional<uint8_t> {
    auto realKey = decodeSlot(room, name, genreLevel());
    if (!realKey) {
        return std::nullopt;
    }
    return static_cast<uint8_t>(*realKey);
}

auto artistSlotFor(const std::string& room, const std::string& name) -> std::optional<ArtistSlot> {
    auto realKey = decodeSlot(room, name, artistLevel());
    if (!realKey) {
        return std::nullopt;
    }
    uint64_t k = *realKey;
    return ArtistSlot{
        static_cast<uint8_t>(k / LibraryConstants::SHELVES_PER_WALL),
        static_cast<uint8_t>(k % LibraryConstants::SHELVES_PER_WALL),
    };
}

auto albumSlotFor(const std::string& room, const std::string& name) -> std::optional<AlbumSlot> {
    auto realKey = decodeSlot(room, name, albumLevel());
    if (!realKey) {
        return std::nullopt;
    }
    uint64_t k     = *realKey;
    uint8_t  album = static_cast<uint8_t>(k % LibraryConstants::ALBUMS_PER_SHELF);
    k /= LibraryConstants::ALBUMS_PER_SHELF;
    uint8_t shelf = static_cast<uint8_t>(k % LibraryConstants::SHELVES_PER_WALL);
    uint8_t wall  = static_cast<uint8_t>(k / LibraryConstants::SHELVES_PER_WALL);
    return AlbumSlot{wall, shelf, album};
}

auto trackSlotFor(const std::string& room, const std::string& name) -> std::optional<TrackSlot> {
    auto realKey = decodeSlot(room, name, trackLevel());
    if (!realKey) {
        return std::nullopt;
    }
    uint64_t k     = *realKey;
    uint8_t  track = static_cast<uint8_t>(k % LibraryConstants::TRACKS_PER_ALBUM);
    k /= LibraryConstants::TRACKS_PER_ALBUM;
    uint8_t album = static_cast<uint8_t>(k % LibraryConstants::ALBUMS_PER_SHELF);
    k /= LibraryConstants::ALBUMS_PER_SHELF;
    uint8_t shelf = static_cast<uint8_t>(k % LibraryConstants::SHELVES_PER_WALL);
    uint8_t wall  = static_cast<uint8_t>(k / LibraryConstants::SHELVES_PER_WALL);
    return TrackSlot{wall, shelf, album, track};
}

} // namespace AudioBabel::IndexNaming
