#include "IndexScramble.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "Constants.h"
#include "Utilities.h"

namespace AudioBabel::IndexScramble {

namespace {

    // bandIndex()/repunit() (length recovery and the base-B repunit), splitmix64()
    // (the avalanche bit mixer used to derive round keys), and the big-integer
    // Feistel primitive feistelPow2() are shared with Index's payload bijection and
    // IndexNaming's cosmetic name generation — see Utilities.h.
    using AudioBabel::Utilities::bandIndex;
    using AudioBabel::Utilities::feistelPow2;
    using AudioBabel::Utilities::repunit;
    using AudioBabel::Utilities::splitmix64;

    // Per-round key for the per-band content Feistel, derived from (seed, band L,
    // round). Distinct mixing constants keep these keys disjoint from the
    // power-of-two sub-Feistel keys (subRoundKey) used by lengthSpread.
    auto contentRoundKey(uint64_t seed, size_t L, int round) -> uint64_t {
        uint64_t state =
            seed ^ (0x9E3779B97F4A7C15ULL * (static_cast<uint64_t>(L) + 1)) ^ (0xD1B54A32D192ED03ULL * (static_cast<uint64_t>(round) + 1));
        return splitmix64(state);
    }

    // contentScramble: length-preserving DIFFUSING permutation within the index's
    // band. Each band has exactly B^L = 2^(16L) elements, so a balanced Feistel
    // over the 16L-bit in-band value is a bijection (16L is always even). Unlike a
    // plain XOR mask — which is affine and so leaves x and x+1 differing only in
    // their low bits — the Feistel scatters neighbours: two payloads that differ
    // only in their last sample land far apart, with different sample values
    // throughout. Inverted by running the Feistel rounds in reverse (encrypt=false).
    auto contentScramble(const cpp_int& index, uint64_t seed, bool encrypt) -> cpp_int {
        if (index <= 0) {
            return cpp_int(0);
        }
        size_t  L  = bandIndex(index);
        cpp_int S  = repunit(L);
        cpp_int y  = index - S; // in [0, 2^(16L))
        cpp_int yp = feistelPow2(y, L * DEFAULT_BIT_DEPTH, [&](int round) { return contentRoundKey(seed, L, round); }, encrypt);
        return S + yp;
    }

    // --- Power-of-two Feistel (used to pick sample values within a target band) -

    // Per-round key for the power-of-two Feistel. Distinct mixing constants keep
    // these keys disjoint from the per-band contentRoundKey() used by contentScramble.
    auto subRoundKey(uint64_t seed, uint64_t subkey, int round) -> uint64_t {
        uint64_t state = seed ^ (0xD6E8FEB86659FD93ULL * (subkey + 1)) ^ (0xA0761D6478BD642FULL * (static_cast<uint64_t>(round) + 1));
        return splitmix64(state);
    }

    // Per-band Feistel over [0, 2^e): thin wrapper around the shared
    // Utilities::feistelPow2 driver, supplying this module's keyed round keys.
    auto feistelBand(const cpp_int& z, size_t e, uint64_t seed, uint64_t subkey, bool encrypt) -> cpp_int {
        return feistelPow2(z, e, [&](int round) { return subRoundKey(seed, subkey, round); }, encrypt);
    }

    // --- Length-spread configuration -------------------------------------------
    // See IndexScramble.h for the full description. The length-spread swaps the
    // short-index PREFIX [1, 2^kPrefixBits) with a set of TARGET slots scattered
    // across kLengthCount distinct, log-spaced bands in [kMinSamples, kMaxSamples].

    constexpr uint32_t kMinSamples = static_cast<uint32_t>(static_cast<uint64_t>(AUDIOBABEL_SCRAMBLE_MIN_MS) * DEFAULT_SAMPLE_RATE / 1000);
    constexpr uint32_t kMaxSamples = static_cast<uint32_t>(static_cast<uint64_t>(AUDIOBABEL_SCRAMBLE_MAX_MS) * DEFAULT_SAMPLE_RATE / 1000);

    constexpr size_t kLengthCountLog2 = AUDIOBABEL_SCRAMBLE_LENGTH_COUNT_LOG2;
    constexpr size_t kLengthCount     = static_cast<size_t>(1) << kLengthCountLog2;
    constexpr size_t kPrefixBits      = AUDIOBABEL_SCRAMBLE_PREFIX_BITS;
    constexpr size_t kSubBits         = kPrefixBits - kLengthCountLog2; // Feistel width for in-target value

    // Subkey for the whole-prefix mixing Feistel. Held well clear of the per-band
    // subkeys [0, kLengthCount) so the two key streams never coincide.
    constexpr uint64_t kPrefixSubkey = 0x5052454649583A31ULL; // "PREFIX:1"

    // Highest band a PREFIX index can occupy: n < 2^kPrefixBits implies
    // bandIndex(n) <= kPrefixBits/16 + 1. The smallest target length must exceed
    // that so PREFIX and TARGET sets are disjoint (the swap stays an involution).
    constexpr size_t kMaxPrefixBand = (kPrefixBits / DEFAULT_BIT_DEPTH) + 1;

    static_assert(kPrefixBits > kLengthCountLog2, "AUDIOBABEL_SCRAMBLE_PREFIX_BITS must exceed LENGTH_COUNT_LOG2");
    static_assert(kPrefixBits % 2 == 0, "AUDIOBABEL_SCRAMBLE_PREFIX_BITS must be even (whole-prefix Feistel domain)");
    static_assert(kSubBits % 2 == 0, "AUDIOBABEL_SCRAMBLE_PREFIX_BITS - LENGTH_COUNT_LOG2 must be even (Feistel domain)");
    static_assert(kMinSamples > 0, "AUDIOBABEL_SCRAMBLE_MIN_MS must be > 0");
    static_assert(kMinSamples < kMaxSamples, "AUDIOBABEL_SCRAMBLE_MIN_MS must be < AUDIOBABEL_SCRAMBLE_MAX_MS");
    static_assert(kMinSamples > kMaxPrefixBand,
                  "Smallest target length must exceed the diversified prefix band range (raise MIN_MS or lower PREFIX_BITS)");

    // kLengthCount distinct target bands (sample counts), log-spaced across
    // [kMinSamples, kMaxSamples], ascending and strictly increasing. Built once.
    auto targetBands() -> const std::array<uint32_t, kLengthCount>& {
        static const std::array<uint32_t, kLengthCount> table = [] {
            std::array<uint32_t, kLengthCount> a{};
            const double                       ratio = static_cast<double>(kMaxSamples) / static_cast<double>(kMinSamples);
            uint32_t                           prev  = 0;
            for (size_t k = 0; k < kLengthCount; ++k) {
                double frac = (kLengthCount == 1) ? 0.0 : static_cast<double>(k) / static_cast<double>(kLengthCount - 1);
                auto   v    = static_cast<uint32_t>(std::llround(static_cast<double>(kMinSamples) * std::pow(ratio, frac)));
                if (k == kLengthCount - 1) {
                    v = kMaxSamples; // pin the top so MAX_MS is hit exactly
                }
                if (v <= prev) {
                    v = prev + 1; // enforce strictly increasing / distinct
                }
                a[k] = v;
                prev = v;
            }
            return a;
        }();
        return table;
    }

    // Position of band L in targetBands() (ascending), or -1 if L is not a target.
    auto targetIndexOfBand(size_t L) -> int {
        const auto& t  = targetBands();
        auto        it = std::lower_bound(t.begin(), t.end(), static_cast<uint32_t>(L));
        if (it != t.end() && *it == static_cast<uint32_t>(L)) {
            return static_cast<int>(it - t.begin());
        }
        return -1;
    }

    // Reverse the low kLengthCountLog2 bits of v. Used so numerically adjacent
    // short indices select FAR-apart target lengths instead of neighbouring ones.
    auto bitReverse(uint64_t v) -> uint64_t {
        uint64_t r = 0;
        for (size_t i = 0; i < kLengthCountLog2; ++i) {
            r = (r << 1) | (v & 1U);
            v >>= 1;
        }
        return r;
    }

    // Keyed involution that swaps the short-index PREFIX with the spread TARGET
    // slots (see IndexScramble.h). Being a fixed pairing of two equal-size,
    // disjoint sets, it is its own inverse, so scramble and unscramble both call
    // it directly.
    auto lengthSpread(const cpp_int& index, uint64_t seed) -> cpp_int {
        if (index <= 0) {
            return cpp_int(0);
        }

        const cpp_int prefixSize = cpp_int(1) << kPrefixBits; // 2^P
        const cpp_int subSize    = cpp_int(1) << kSubBits;    // 2^(P - log2 T), the per-target value domain

        // Case A: a short PREFIX index [1, 1 + 2^P) -> a spread TARGET slot.
        if (index < cpp_int(1) + prefixSize) {
            cpp_int r = index - 1; // in [0, 2^P)
            // Mix the whole value first so STRUCTURED inputs (e.g. browse indices,
            // which step by a fixed stride) still spread evenly across lengths
            // instead of cycling through a couple of values of the low bits.
            cpp_int rp  = feistelBand(r, kPrefixBits, seed, kPrefixSubkey, /*encrypt=*/true);
            auto    j   = static_cast<uint64_t>(rp & ((cpp_int(1) << kLengthCountLog2) - 1)); // rp mod T
            cpp_int o   = rp >> kLengthCountLog2;                                             // rp / T, in [0, 2^kSubBits)
            size_t  p   = static_cast<size_t>(bitReverse(j));                                 // spread adjacent j across bands
            size_t  L   = targetBands()[p];
            cpp_int off = feistelBand(o, kSubBits, seed, p, /*encrypt=*/true); // in [0, 2^kSubBits) < B^L
            return repunit(L) + off;
        }

        // Case B: a TARGET slot -> back down into the PREFIX block.
        size_t L = bandIndex(index);
        int    p = targetIndexOfBand(L);
        if (p >= 0) {
            cpp_int off = index - repunit(L); // in-band offset
            if (off < subSize) {              // exactly the slots the spread uses
                cpp_int  o  = feistelBand(off, kSubBits, seed, static_cast<size_t>(p), /*encrypt=*/false);
                uint64_t j  = bitReverse(static_cast<uint64_t>(p));
                cpp_int  rp = (o << kLengthCountLog2) | cpp_int(j);
                cpp_int  r  = feistelBand(rp, kPrefixBits, seed, kPrefixSubkey, /*encrypt=*/false);
                return r + 1;
            }
        }

        // Case C: already a non-trivial length -> unchanged.
        return index;
    }

} // namespace

auto scramble(const cpp_int& index, uint64_t seed) -> cpp_int {
    if (index <= 0) {
        return cpp_int(0); // 0 (and only 0) is the empty payload; keep it fixed
    }
    cpp_int content = contentScramble(index, seed, /*encrypt=*/true);
    return lengthSpread(content, seed);
}

auto unscramble(const cpp_int& index, uint64_t seed) -> cpp_int {
    if (index <= 0) {
        return cpp_int(0);
    }
    // Undo in reverse: lengthSpread is an involution, then invert the content scramble.
    cpp_int content = lengthSpread(index, seed);
    return contentScramble(content, seed, /*encrypt=*/false);
}

auto config() -> Config& {
    static Config cfg{kScrambleEnabledByDefault, static_cast<uint64_t>(AUDIOBABEL_SCRAMBLE_SEED)};
    return cfg;
}

} // namespace AudioBabel::IndexScramble
