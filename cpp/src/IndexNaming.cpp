#include "../include/IndexNaming.h"

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <iterator>
#include <optional>
#include <random>
#include <vector>

#include "../include/Constants.h"
#include "../include/LibraryPosition.h"
#include "../include/Utilities.h"

namespace AudioBabel::IndexNaming {

namespace mp = boost::multiprecision;

namespace {

    // Global permutation key for the name-material Feistel. Deliberately a
    // single fixed constant (not room- or coordinate-keyed): names must be a
    // universal function of the index alone, so that the same index always
    // yields the same names and so that names can be inverted into indexes
    // without first knowing where the index lives. Distinct from the salts
    // IndexScramble uses so the two features' keying can never alias.
    constexpr uint64_t NAME_KEY = 0xA24BAED4963EE407ULL;

    using AudioBabel::Utilities::feistelPow2;
    using AudioBabel::Utilities::splitmix64;

    // D = number of distinct names per field = count of non-empty base64
    // strings of length 1..NAME_MAX_CHARS = sum_{L=1..NAME_MAX_CHARS} 64^L.
    // FULL = D^4 spans every (genre,artist,album,track) name combination.
    // E = smallest even bit-width with 2^E >= FULL, for the Feistel domain.
    struct NameSpace {
        cpp_int d;    // names per field
        cpp_int full; // d^4
        size_t  e;    // even bits covering full
    };

    auto nameSpace() -> const NameSpace& {
        static const NameSpace ns = [] {
            cpp_int d = 0;
            cpp_int p = 1;
            for (size_t i = 0; i < NAME_MAX_CHARS; ++i) {
                p *= 64;
                d += p;
            }
            cpp_int full = d * d * d * d;

            // Smallest even e with 2^e >= full.
            size_t bits = static_cast<size_t>(mp::msb(full));               // floor(log2)
            size_t pe   = ((cpp_int(1) << bits) == full) ? bits : bits + 1; // ceil(log2)
            size_t e    = (pe % 2 == 0) ? pe : pe + 1;

            return NameSpace{d, full, e};
        }();
        return ns;
    }

    // Sample a cpp_int uniformly in [0, d) using enough random bits to keep
    // modulo bias negligible (32 extra bits → bias < 2^-32).
    auto sampleBelow(std::mt19937_64& rng, const cpp_int& d) -> cpp_int {
        size_t  bits  = static_cast<size_t>(mp::msb(d)) + 1;
        size_t  words = (bits + 32 + 63) / 64;
        cpp_int r     = 0;
        for (size_t w = 0; w < words; ++w) {
            r = (r << 64) | cpp_int(rng());
        }
        return r % d;
    }

    auto roundKey(uint64_t key, int round) -> uint64_t {
        uint64_t state = key ^ (0x9E3779B97F4A7C15ULL * (static_cast<uint64_t>(round) + 1));
        return splitmix64(state);
    }

    // Name-material Feistel over [0, 2^e): thin wrapper around the shared
    // Utilities::feistelPow2 driver, keyed by the fixed global NAME_KEY.
    auto feistel(const cpp_int& x, size_t e, bool encrypt) -> cpp_int {
        return feistelPow2(
            x, e, [](int round) { return roundKey(NAME_KEY, round); }, encrypt);
    }

    // Keyed bijection on [0, full) via feistel() + cycle-walking (Black &
    // Rogaway, CT-RSA 2002): re-apply the power-of-two permutation until the
    // result lands back in [0, full).
    auto permuteEncode(const cpp_int& x) -> cpp_int {
        const NameSpace& ns = nameSpace();
        cpp_int          y  = feistel(x, ns.e, /*encrypt=*/true);
        while (y >= ns.full) {
            y = feistel(y, ns.e, /*encrypt=*/true);
        }
        return y;
    }

    auto permuteDecode(const cpp_int& y) -> cpp_int {
        const NameSpace& ns = nameSpace();
        cpp_int          x  = feistel(y, ns.e, /*encrypt=*/false);
        while (x >= ns.full) {
            x = feistel(x, ns.e, /*encrypt=*/false);
        }
        return x;
    }

    // A field value f in [0, D) renders to a 1..NAME_MAX_CHARS bijective base64
    // string (f + 1 skips the empty string that value 0 would produce).
    auto nameForField(const cpp_int& f) -> std::string {
        return AudioBabel::Utilities::indexToB64(f + 1);
    }

    // Parse a field name back to its value, or nullopt if it is not a name this
    // module could have produced (too long, or an invalid character).
    auto fieldForName(const std::string& name) -> std::optional<cpp_int> {
        if (name.empty() || name.size() > NAME_MAX_CHARS) {
            return std::nullopt;
        }
        if (!AudioBabel::Utilities::isValidBase64Url(name)) {
            return std::nullopt;
        }
        cpp_int value = AudioBabel::Utilities::b64ToIndex(name); // >= 1 for non-empty
        if (value < 1 || value > nameSpace().d) {
            return std::nullopt;
        }
        return value - 1;
    }

    // Split a permuted name-material value into its four base-D field digits,
    // least-significant first: {genre, artist, album, track}.
    auto splitFields(const cpp_int& scrambled) -> std::array<cpp_int, 4> {
        const cpp_int&         d = nameSpace().d;
        std::array<cpp_int, 4> f{};
        cpp_int                t = scrambled;
        for (auto& digit : f) {
            digit = t % d;
            t /= d;
        }
        return f;
    }

    // Inverse of splitFields: combine four field digits into name material.
    auto combineFields(const std::array<cpp_int, 4>& f) -> cpp_int {
        const cpp_int& d = nameSpace().d;
        cpp_int        s = 0;
        for (size_t i = 4; i-- > 0;) {
            s = s * d + f[i];
        }
        return s;
    }

    // The metadata name an index carries for one Browse sibling slot: name the
    // representative index at that position (deeper coordinates already 0).
    auto nameAt(const LibraryPosition& pos, char level) -> std::string {
        cpp_int index = reconstructIndexFromPosition(pos);
        Names   names = namesForIndex(index);
        switch (level) {
            case 'g':
                return names.genre;
            case 'r':
                return names.artist;
            case 'b':
                return names.album;
            default:
                return names.track;
        }
    }

} // namespace

auto nameMaxChars() -> size_t {
    return NAME_MAX_CHARS;
}

auto namesForIndex(const cpp_int& index) -> Names {
    cpp_int idx = index < 0 ? cpp_int(0) : index;
    cpp_int m   = idx % nameSpace().full; // name material (low part)
    cpp_int s   = permuteEncode(m);

    std::array<cpp_int, 4> f = splitFields(s);
    Names                  names;
    names.genre  = nameForField(f[0]);
    names.artist = nameForField(f[1]);
    names.album  = nameForField(f[2]);
    names.track  = nameForField(f[3]);
    return names;
}

auto constructIndexesForNames(const NameQuery& query, size_t count, uint64_t seed) -> std::vector<cpp_int> {
    // Parse every pinned field up front; a single unproducible name means the
    // whole request has no answers.
    std::array<std::optional<cpp_int>, 4>                  pinned{};
    const std::array<const std::optional<std::string>*, 4> inputs = {&query.genre, &query.artist, &query.album, &query.track};
    for (size_t i = 0; i < 4; ++i) {
        if (inputs[i]->has_value()) {
            auto parsed = fieldForName(**inputs[i]);
            if (!parsed) {
                return {};
            }
            pinned[i] = *parsed;
        }
    }

    const NameSpace& ns = nameSpace();
    std::mt19937_64  rng(seed);

    // Discriminator must be large enough to produce indexes that yield
    // meaningful audio. Target ~65536 base64 chars (6 bits each = 393216 bits).
    // discWords = ceil((targetBits - bits(FULL)) / 64), minimum 1.
    static const size_t TARGET_INDEX_BITS = 65536 * 6;
    const size_t        fullBits          = static_cast<size_t>(mp::msb(ns.full)) + 1;
    const size_t        discBits          = TARGET_INDEX_BITS > fullBits ? TARGET_INDEX_BITS - fullBits : 1;
    const size_t        discWords         = (discBits + 63) / 64;

    std::vector<cpp_int> results;
    results.reserve(count);
    for (size_t k = 0; k < count; ++k) {
        std::array<cpp_int, 4> f{};
        for (size_t i = 0; i < 4; ++i) {
            f[i] = pinned[i] ? *pinned[i] : sampleBelow(rng, ns.d);
        }
        cpp_int material = permuteDecode(combineFields(f));
        // High "discriminator" bits make each result a distinct candidate even
        // when every field is pinned. Generated with enough bits for long audio.
        cpp_int discriminator = 0;
        for (size_t w = 0; w < discWords; ++w) {
            discriminator = (discriminator << 64) | cpp_int(rng());
        }
        results.push_back(discriminator * ns.full + material);
    }
    return results;
}

auto genreNames(const std::string& room) -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(LibraryConstants::WALLS_PER_ROOM);
    for (uint8_t wall = 0; wall < LibraryConstants::WALLS_PER_ROOM; ++wall) {
        names.push_back(nameAt(LibraryPosition{room, wall, 0, 0, 0}, 'g'));
    }
    return names;
}

auto artistNames(const std::string& room, uint8_t wall) -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(LibraryConstants::SHELVES_PER_WALL);
    for (uint8_t shelf = 0; shelf < LibraryConstants::SHELVES_PER_WALL; ++shelf) {
        names.push_back(nameAt(LibraryPosition{room, wall, shelf, 0, 0}, 'r'));
    }
    return names;
}

auto albumNames(const std::string& room, uint8_t wall, uint8_t shelf) -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(LibraryConstants::ALBUMS_PER_SHELF);
    for (uint8_t album = 0; album < LibraryConstants::ALBUMS_PER_SHELF; ++album) {
        names.push_back(nameAt(LibraryPosition{room, wall, shelf, album, 0}, 'b'));
    }
    return names;
}

auto trackNames(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album) -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(LibraryConstants::TRACKS_PER_ALBUM);
    for (uint8_t track = 0; track < LibraryConstants::TRACKS_PER_ALBUM; ++track) {
        names.push_back(nameAt(LibraryPosition{room, wall, shelf, album, track}, 't'));
    }
    return names;
}

} // namespace AudioBabel::IndexNaming
