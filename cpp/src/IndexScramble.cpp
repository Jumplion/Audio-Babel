#include "IndexScramble.h"

#include <array>
#include <cstdint>
#include <vector>

#include "Constants.h"

namespace AudioBabel::IndexScramble {

namespace mp = boost::multiprecision;

namespace {

constexpr uint32_t SAMPLE_BASE  = SAMPLE_ALPHABET_SIZE;        // B = 65536
constexpr unsigned SAMPLE_BITS  = DEFAULT_BIT_DEPTH;           // 16
constexpr size_t   SAMPLE_BYTES = DEFAULT_BIT_DEPTH / BITS_PER_BYTE; // 2

// Number of Feistel rounds. Four rounds give full avalanche across the band.
constexpr int FEISTEL_ROUNDS = 4;

// A small, fast bit mixer (one SplitMix64 step on a running state).
inline void mixIn(uint64_t& state, uint8_t x) {
    state += x + 0x9E3779B97F4A7C15ULL;
    state = (state ^ (state >> 30)) * 0xBF58476D1CE4E5B9ULL;
    state = (state ^ (state >> 27)) * 0x94D049BB133111EBULL;
    state ^= state >> 31;
}

inline auto splitmix64(uint64_t& state) -> uint64_t {
    uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z          = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z          = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

// Band index L (== sample count) for a stored index value. Mirrors the
// magnitude-based length recovery used by indexToAudioData.
auto bandIndex(const cpp_int& n) -> size_t {
    if (n == 0) {
        return 0;
    }
    cpp_int m = (n * (SAMPLE_BASE - 1)) + 1;
    return static_cast<size_t>(mp::msb(m) / SAMPLE_BITS);
}

// Base-B repunit S_L = (B^L - 1)/(B - 1): byte pattern 0x00 0x01 repeated L times.
auto repunit(size_t L) -> cpp_int {
    if (L == 0) {
        return cpp_int(0);
    }
    std::vector<uint8_t> bytes(L * SAMPLE_BYTES, 0);
    for (size_t i = 0; i < L; ++i) {
        bytes[(i * SAMPLE_BYTES) + 1] = 0x01;
    }
    cpp_int s = 0;
    mp::import_bits(s, bytes.begin(), bytes.end(), BITS_PER_BYTE, true);
    return s;
}

// Per-round key derived from the seed, band and round index.
auto roundKey(uint64_t seed, size_t L, int round) -> uint64_t {
    uint64_t state = seed ^ (0x9E3779B97F4A7C15ULL * (static_cast<uint64_t>(L) + 1)) ^ (0xD1B54A32D192ED03ULL * (static_cast<uint64_t>(round) + 1));
    return splitmix64(state);
}

// Keyed diffusing round function: half-block -> half-block of the same length.
// Each output byte depends on every input byte (forward pass spreads low->high,
// backward pass spreads high->low). It need not be invertible — the Feistel
// structure provides invertibility.
auto roundFunction(const std::vector<uint8_t>& in, uint64_t key) -> std::vector<uint8_t> {
    const size_t        n = in.size();
    std::vector<uint8_t> out(n, 0);

    uint64_t fwd = key ^ 0xA0761D6478BD642FULL;
    for (size_t i = 0; i < n; ++i) {
        mixIn(fwd, in[i]);
        out[i] = static_cast<uint8_t>(fwd);
    }

    uint64_t bwd = key ^ 0xE7037ED1A0B428DBULL;
    for (size_t i = n; i-- > 0;) {
        mixIn(bwd, static_cast<uint8_t>(in[i] ^ out[i]));
        out[i] = static_cast<uint8_t>(out[i] ^ static_cast<uint8_t>(bwd >> 17));
    }
    return out;
}

// Apply (or invert) the keyed Feistel permutation over the band value `y`,
// represented as exactly 2L bytes (most-significant first). Halves are L bytes.
auto feistel(const cpp_int& y, size_t L, uint64_t seed, bool encrypt) -> cpp_int {
    std::vector<uint8_t> raw;
    mp::export_bits(y, std::back_inserter(raw), BITS_PER_BYTE, true);

    std::vector<uint8_t> bytes(2 * L, 0); // left-pad to the full band width
    if (raw.size() <= bytes.size()) {
        std::copy(raw.begin(), raw.end(), bytes.end() - static_cast<std::ptrdiff_t>(raw.size()));
    }

    std::vector<uint8_t> hi(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(L));
    std::vector<uint8_t> lo(bytes.begin() + static_cast<std::ptrdiff_t>(L), bytes.end());

    if (encrypt) {
        for (int r = 0; r < FEISTEL_ROUNDS; ++r) {
            std::vector<uint8_t> f = roundFunction(lo, roundKey(seed, L, r));
            for (size_t i = 0; i < L; ++i) {
                uint8_t t = static_cast<uint8_t>(hi[i] ^ f[i]);
                hi[i]     = lo[i];
                lo[i]     = t;
            }
        }
    } else {
        for (int r = FEISTEL_ROUNDS - 1; r >= 0; --r) {
            std::vector<uint8_t> f = roundFunction(hi, roundKey(seed, L, r));
            for (size_t i = 0; i < L; ++i) {
                uint8_t t = static_cast<uint8_t>(lo[i] ^ f[i]);
                lo[i]     = hi[i];
                hi[i]     = t;
            }
        }
    }

    std::vector<uint8_t> outBytes;
    outBytes.reserve(2 * L);
    outBytes.insert(outBytes.end(), hi.begin(), hi.end());
    outBytes.insert(outBytes.end(), lo.begin(), lo.end());

    cpp_int result = 0;
    mp::import_bits(result, outBytes.begin(), outBytes.end(), BITS_PER_BYTE, true);
    return result;
}

} // namespace

auto scramble(const cpp_int& index, uint64_t seed) -> cpp_int {
    if (index <= 0) {
        return cpp_int(0); // 0 (and only 0) lives in band L == 0
    }
    size_t L = bandIndex(index);
    if (L == 0) {
        return index;
    }
    cpp_int S = repunit(L);
    cpp_int y = index - S; // in [0, 2^(16L))
    return S + feistel(y, L, seed, /*encrypt=*/true);
}

auto unscramble(const cpp_int& index, uint64_t seed) -> cpp_int {
    if (index <= 0) {
        return cpp_int(0);
    }
    size_t L = bandIndex(index);
    if (L == 0) {
        return index;
    }
    cpp_int S  = repunit(L);
    cpp_int y2 = index - S;
    return S + feistel(y2, L, seed, /*encrypt=*/false);
}

auto config() -> Config& {
    static Config cfg{kScrambleEnabledByDefault, static_cast<uint64_t>(AUDIOBABEL_SCRAMBLE_SEED)};
    return cfg;
}

auto applyScramble(const cpp_int& index) -> cpp_int {
    const Config& cfg = config();
    return cfg.enabled ? scramble(index, cfg.seed) : index;
}

auto applyUnscramble(const cpp_int& index) -> cpp_int {
    const Config& cfg = config();
    return cfg.enabled ? unscramble(index, cfg.seed) : index;
}

} // namespace AudioBabel::IndexScramble
