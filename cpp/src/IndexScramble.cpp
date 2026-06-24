#include "IndexScramble.h"

#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <vector>

#include "Constants.h"
#include "Utilities.h"

namespace AudioBabel::IndexScramble {

namespace mp = boost::multiprecision;

namespace {

    // Number of Feistel rounds. Four rounds give full avalanche across the band.
    constexpr int FEISTEL_ROUNDS = 4;

    // bandIndex()/repunit() (length recovery and the base-B repunit) and
    // mixIn()/splitmix64() (the avalanche bit mixer) are shared with Index's
    // payload bijection and IndexNaming's cosmetic name generation — see
    // Utilities.h.
    using AudioBabel::Utilities::bandIndex;
    using AudioBabel::Utilities::mixIn;
    using AudioBabel::Utilities::repunit;
    using AudioBabel::Utilities::splitmix64;

    // Per-round key derived from the seed, band and round index.
    auto roundKey(uint64_t seed, size_t L, int round) -> uint64_t {
        uint64_t state =
            seed ^ (0x9E3779B97F4A7C15ULL * (static_cast<uint64_t>(L) + 1)) ^ (0xD1B54A32D192ED03ULL * (static_cast<uint64_t>(round) + 1));
        return splitmix64(state);
    }

    // Keyed diffusing round function: half-block -> half-block of the same length.
    // Each output byte depends on every input byte (forward pass spreads low->high,
    // backward pass spreads high->low). It need not be invertible — the Feistel
    // structure provides invertibility.
    auto roundFunction(const std::vector<uint8_t>& in, uint64_t key) -> std::vector<uint8_t> {
        const size_t         n = in.size();
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

    // --- Tiered cross-band scramble ---------------------------------------------
    // See IndexScramble.h for why tiers exist. feistel() above stays inside one
    // length-band; the functions below instead permute across a whole tier (a
    // contiguous run of bands), bounded by kTierMaxSamples.
    //
    // Tier i (1-based) covers sample counts (kTierMaxSamples[i-2], kTierMaxSamples[i-1]]
    // with the first tier starting at 1 sample. Anything longer than the last entry
    // keeps the original length-preserving feistel().

    // Tier boundaries in seconds, configurable at compile time via
    // AUDIOBABEL_SCRAMBLE_TIER_SECONDS (see IndexScramble.h). Defaults to 4 tiers
    // at 1, 5, 10, 15 seconds. The array size is derived from the macro itself
    // (via the raw C array below) so overriding the macro with a different
    // number of tiers doesn't require also updating a hardcoded size here.
    constexpr uint64_t kTierSecondsRaw[]  = AUDIOBABEL_SCRAMBLE_TIER_SECONDS;
    constexpr size_t   kNumTiers          = std::size(kTierSecondsRaw);
    constexpr std::array<uint64_t, kNumTiers> kTierSeconds = [] {
        std::array<uint64_t, kNumTiers> a{};
        for (size_t i = 0; i < kNumTiers; ++i) {
            a[i] = kTierSecondsRaw[i];
        }
        return a;
    }();

    template <typename Container>
    constexpr auto isStrictlyIncreasing(const Container& values) -> bool {
        constexpr size_t N    = std::tuple_size<std::decay_t<Container>>::value;
        auto             data = values.data();
        for (size_t i = 1; i < N; ++i) {
            if (!(*(data + i - 1) < *(data + i))) {
                return false;
            }
        }
        return true;
    }

    static_assert(isStrictlyIncreasing(kTierSeconds), "AUDIOBABEL_SCRAMBLE_TIER_SECONDS must be non-empty and strictly increasing");

    template <typename Container>
    auto secondsToMaxSamples(const Container& seconds) -> std::array<uint32_t, std::tuple_size<std::decay_t<Container>>::value> {
        constexpr size_t        N = std::tuple_size<std::decay_t<Container>>::value;
        std::array<uint32_t, N> result{};
        for (size_t i = 0; i < N; ++i) {
            result[i] = static_cast<uint32_t>(seconds[i]) * DEFAULT_SAMPLE_RATE;
        }
        return result;
    }

    // Per-tier maximum sample count at DEFAULT_SAMPLE_RATE, derived from kTierSeconds.
    const auto kTierMaxSamples = secondsToMaxSamples(kTierSeconds);

    // Tier (1-based) containing sample-band L, or 0 if L is beyond the last tier
    // (those keep the legacy length-preserving scramble). L is assumed >= 1.
    auto tierForBand(size_t L) -> size_t {
        for (size_t i = 0; i < kTierMaxSamples.size(); ++i) {
            if (L <= kTierMaxSamples[i]) {
                return i + 1;
            }
        }
        return 0;
    }

    // Per-round key for the tiered Feistel. Distinct mixing constants keep these
    // keys disjoint from the per-band roundKey() used by the legacy path.
    auto tierRoundKey(uint64_t seed, uint64_t tier, int round) -> uint64_t {
        uint64_t state = seed ^ (0xD6E8FEB86659FD93ULL * (tier + 1)) ^ (0xA0761D6478BD642FULL * (static_cast<uint64_t>(round) + 1));
        return splitmix64(state);
    }

    // Low-h-bits mask as a big integer.
    auto lowBitsMask(size_t h) -> cpp_int {
        return (cpp_int(1) << h) - 1;
    }

    // h-bit keyed diffusing round function built on the byte-oriented roundFunction.
    // The h-bit half is laid out in ceil(h/8) bytes (most-significant first) and the
    // result is masked back to h bits, so it maps an h-bit value to an h-bit value.
    // `mask` is the caller's already-computed lowBitsMask(h), passed in so the
    // (otherwise identical) mask isn't rebuilt on every one of the four rounds.
    auto roundFunctionBits(const cpp_int& half, size_t h, uint64_t key, const cpp_int& mask) -> cpp_int {
        const size_t         hbytes = (h + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
        std::vector<uint8_t> in(hbytes, 0);

        std::vector<uint8_t> raw;
        mp::export_bits(half, std::back_inserter(raw), BITS_PER_BYTE, true);
        if (raw.size() <= in.size()) {
            std::copy(raw.begin(), raw.end(), in.end() - static_cast<std::ptrdiff_t>(raw.size()));
        }

        std::vector<uint8_t> out = roundFunction(in, key);
        cpp_int              r   = 0;
        mp::import_bits(r, out.begin(), out.end(), BITS_PER_BYTE, true);
        return r & mask;
    }

    // Keyed balanced Feistel permutation over [0, 2^e) (e even), built from big
    // integers so the domain need not be byte-aligned. Halves are e/2 bits each.
    // Inverted by running the rounds in reverse, exactly like feistel().
    auto feistelPow2(const cpp_int& z, size_t e, uint64_t seed, uint64_t tier, bool encrypt) -> cpp_int {
        const size_t  h    = e / 2;
        const cpp_int mask = lowBitsMask(h);
        cpp_int       hi   = (z >> h) & mask;
        cpp_int       lo   = z & mask;

        if (encrypt) {
            for (int r = 0; r < FEISTEL_ROUNDS; ++r) {
                cpp_int f = roundFunctionBits(lo, h, tierRoundKey(seed, tier, r), mask);
                cpp_int t = hi ^ f;
                hi        = lo;
                lo        = t;
            }
        } else {
            for (int r = FEISTEL_ROUNDS - 1; r >= 0; --r) {
                cpp_int f = roundFunctionBits(hi, h, tierRoundKey(seed, tier, r), mask);
                cpp_int t = lo ^ f;
                lo        = hi;
                hi        = t;
            }
        }
        return (hi << h) | lo;
    }

    // Geometry of a tier: low end Lo (a repunit), width N, and the even bit-width e
    // of the smallest power-of-two domain covering N (so the Feistel splits evenly
    // and cycle-walking stays under ~4x expansion).
    struct TierGeometry {
        cpp_int  lo;
        cpp_int  n;
        size_t   e;
        uint64_t tier;
    };

    auto computeTierGeometry(size_t tier) -> TierGeometry {
        const uint32_t highBand = kTierMaxSamples[tier - 1];
        const uint32_t lowBand  = (tier == 1) ? 1U : (kTierMaxSamples[tier - 2] + 1U);

        cpp_int lo = repunit(lowBand);      // S_lowBand
        cpp_int hi = repunit(highBand + 1); // S_(highBand+1), exclusive upper end
        cpp_int n  = hi - lo;

        // Smallest even e with 2^e >= n.
        size_t bits = static_cast<size_t>(mp::msb(n));               // floor(log2 n)
        size_t p    = ((cpp_int(1) << bits) == n) ? bits : bits + 1; // ceil(log2 n)
        size_t e    = (p % 2 == 0) ? p : p + 1;

        return TierGeometry{lo, n, e, static_cast<uint64_t>(tier)};
    }

    // tierGeometry() is pure (depends only on the fixed tier boundaries, never
    // on seed or index), but rebuilding it involves a big-integer subtraction
    // over the whole tier width — cache it per tier instead of redoing that
    // work on every scramble()/unscramble() call.
    auto tierGeometry(size_t tier) -> const TierGeometry& {
        static std::array<std::unique_ptr<TierGeometry>, kTierMaxSamples.size()> cache{};
        auto& slot = cache[tier - 1];
        if (!slot) {
            slot = std::make_unique<TierGeometry>(computeTierGeometry(tier));
        }
        return *slot;
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

    size_t tier = tierForBand(L);
    if (tier == 0) {
        // Very long payloads keep the original length-preserving permutation.
        cpp_int S = repunit(L);
        cpp_int y = index - S; // in [0, 2^(16L))
        return S + feistel(y, L, seed, /*encrypt=*/true);
    }

    // Tiered path: permute across the whole tier so neighbouring lengths spread
    // out and short inputs reach the tier's (much larger) maximum length.
    const TierGeometry& g = tierGeometry(tier);
    cpp_int              z = index - g.lo; // in [0, N)
    cpp_int              y = feistelPow2(z, g.e, seed, g.tier, /*encrypt=*/true);
    // Cycle-walking (Black & Rogaway, "Ciphers with Arbitrary Finite Domains",
    // CT-RSA 2002): re-apply the power-of-two bijection until the result lands
    // back in [0, N); see IndexScramble.h "References" for the paper.
    while (y >= g.n) { // cycle-walk back into [0, N)
        y = feistelPow2(y, g.e, seed, g.tier, /*encrypt=*/true);
    }
    return g.lo + y;
}

auto unscramble(const cpp_int& index, uint64_t seed) -> cpp_int {
    if (index <= 0) {
        return cpp_int(0);
    }
    size_t L = bandIndex(index);
    if (L == 0) {
        return index;
    }

    size_t tier = tierForBand(L);
    if (tier == 0) {
        cpp_int S  = repunit(L);
        cpp_int y2 = index - S;
        return S + feistel(y2, L, seed, /*encrypt=*/false);
    }

    const TierGeometry& g = tierGeometry(tier);
    cpp_int             y = index - g.lo; // in [0, N)
    cpp_int             z = feistelPow2(y, g.e, seed, g.tier, /*encrypt=*/false);
    while (z >= g.n) { // cycle-walk back into [0, N)
        z = feistelPow2(z, g.e, seed, g.tier, /*encrypt=*/false);
    }
    return g.lo + z;
}

auto config() -> Config& {
    static Config cfg{kScrambleEnabledByDefault, static_cast<uint64_t>(AUDIOBABEL_SCRAMBLE_SEED)};
    return cfg;
}

} // namespace AudioBabel::IndexScramble
