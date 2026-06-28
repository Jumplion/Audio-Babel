#include "IndexScramble.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
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

    // contentScramble: length-preserving XOR-stream permutation within the index's
    // band. Each band has exactly B^L = 2^(16L) elements, so XOR is a valid
    // bijection. The keystream is derived from (seed, L) via splitmix64 in counter
    // mode (one step per 8 bytes). XOR is self-inverse, so scramble and unscramble
    // call the same function.
    auto contentScramble(const cpp_int& index, uint64_t seed) -> cpp_int {
        if (index <= 0) {
            return cpp_int(0);
        }
        size_t  L = bandIndex(index);
        cpp_int S = repunit(L);
        cpp_int y = index - S; // in [0, 2^(16L))

        std::vector<uint8_t> raw;
        mp::export_bits(y, std::back_inserter(raw), BITS_PER_BYTE, true);

        std::vector<uint8_t> bytes(2 * L, 0);
        if (raw.size() <= bytes.size()) {
            std::copy(raw.begin(), raw.end(), bytes.end() - static_cast<std::ptrdiff_t>(raw.size()));
        }

        uint64_t state = seed ^ (0x9E3779B97F4A7C15ULL * (static_cast<uint64_t>(L) + 1));
        for (size_t i = 0; i < bytes.size(); i += 8) {
            uint64_t ks    = splitmix64(state);
            size_t   chunk = std::min(static_cast<size_t>(8), bytes.size() - i);
            for (size_t j = 0; j < chunk; ++j) {
                bytes[i + j] ^= static_cast<uint8_t>(ks >> (j * 8));
            }
        }

        cpp_int result = 0;
        mp::import_bits(result, bytes.begin(), bytes.end(), BITS_PER_BYTE, true);
        return S + result;
    }

    // --- Power-of-two Feistel (used to pick sample values within a target band) -

    // Per-round key for the power-of-two Feistel. Distinct mixing constants keep
    // these keys disjoint from the per-band roundKey() used by contentScramble.
    auto subRoundKey(uint64_t seed, uint64_t subkey, int round) -> uint64_t {
        uint64_t state = seed ^ (0xD6E8FEB86659FD93ULL * (subkey + 1)) ^ (0xA0761D6478BD642FULL * (static_cast<uint64_t>(round) + 1));
        return splitmix64(state);
    }

    // Low-h-bits mask as a big integer.
    auto lowBitsMask(size_t h) -> cpp_int {
        return (cpp_int(1) << h) - 1;
    }

    // h-bit keyed diffusing round function built on the byte-oriented roundFunction.
    // The h-bit half is laid out in ceil(h/8) bytes (most-significant first) and the
    // result is masked back to h bits, so it maps an h-bit value to an h-bit value.
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
    // Inverted by running the rounds in reverse.
    auto feistelPow2(const cpp_int& z, size_t e, uint64_t seed, uint64_t subkey, bool encrypt) -> cpp_int {
        const size_t  h    = e / 2;
        const cpp_int mask = lowBitsMask(h);
        cpp_int       hi   = (z >> h) & mask;
        cpp_int       lo   = z & mask;

        if (encrypt) {
            for (int r = 0; r < FEISTEL_ROUNDS; ++r) {
                cpp_int f = roundFunctionBits(lo, h, subRoundKey(seed, subkey, r), mask);
                cpp_int t = hi ^ f;
                hi        = lo;
                lo        = t;
            }
        } else {
            for (int r = FEISTEL_ROUNDS - 1; r >= 0; --r) {
                cpp_int f = roundFunctionBits(hi, h, subRoundKey(seed, subkey, r), mask);
                cpp_int t = lo ^ f;
                lo        = hi;
                hi        = t;
            }
        }
        return (hi << h) | lo;
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
            cpp_int rp  = feistelPow2(r, kPrefixBits, seed, kPrefixSubkey, /*encrypt=*/true);
            auto    j   = static_cast<uint64_t>(rp & ((cpp_int(1) << kLengthCountLog2) - 1)); // rp mod T
            cpp_int o   = rp >> kLengthCountLog2;                                             // rp / T, in [0, 2^kSubBits)
            size_t  p   = static_cast<size_t>(bitReverse(j));                                 // spread adjacent j across bands
            size_t  L   = targetBands()[p];
            cpp_int off = feistelPow2(o, kSubBits, seed, p, /*encrypt=*/true); // in [0, 2^kSubBits) < B^L
            return repunit(L) + off;
        }

        // Case B: a TARGET slot -> back down into the PREFIX block.
        size_t L = bandIndex(index);
        int    p = targetIndexOfBand(L);
        if (p >= 0) {
            cpp_int off = index - repunit(L); // in-band offset
            if (off < subSize) {              // exactly the slots the spread uses
                cpp_int  o  = feistelPow2(off, kSubBits, seed, static_cast<size_t>(p), /*encrypt=*/false);
                uint64_t j  = bitReverse(static_cast<uint64_t>(p));
                cpp_int  rp = (o << kLengthCountLog2) | cpp_int(j);
                cpp_int  r  = feistelPow2(rp, kPrefixBits, seed, kPrefixSubkey, /*encrypt=*/false);
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
    cpp_int content = contentScramble(index, seed);
    return lengthSpread(content, seed);
}

auto unscramble(const cpp_int& index, uint64_t seed) -> cpp_int {
    if (index <= 0) {
        return cpp_int(0);
    }
    cpp_int content = lengthSpread(index, seed);
    return contentScramble(content, seed);
}

auto config() -> Config& {
    static Config cfg{kScrambleEnabledByDefault, static_cast<uint64_t>(AUDIOBABEL_SCRAMBLE_SEED)};
    return cfg;
}

} // namespace AudioBabel::IndexScramble
