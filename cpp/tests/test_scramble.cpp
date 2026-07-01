/**
 * @file test_scramble.cpp
 * @brief Tests for the optional reversible index scramble.
 *
 * Covers the pure transform (bijection, inverse, 0->0, neighbour scatter) and
 * the length-spread that gives short indices a wide, varied range of decoded
 * durations, plus the toggle wired into the encode/decode pipeline (round-trip
 * preserved, output differs when enabled, core untouched when disabled).
 */

#include <Index.h>
#include <IndexScramble.h>

#include <boost/multiprecision/cpp_int.hpp>
#include <catch2/catch_test_macros.hpp>
#include <random>
#include <set>

#include "test_common.h"

using namespace AudioBabel;
using boost::multiprecision::cpp_int;

namespace {

/// RAII guard that sets the scramble config and restores it afterwards.
struct ScrambleGuard {
    IndexScramble::Config saved;
    explicit ScrambleGuard(bool enabled, uint64_t seed) : saved(IndexScramble::config()) {
        IndexScramble::config() = IndexScramble::Config{enabled, seed};
    }
    ~ScrambleGuard() {
        IndexScramble::config() = saved;
    }
};

// The default length-spread bounds (AUDIOBABEL_SCRAMBLE_MIN_MS / MAX_MS = 100 ms
// / 5 s at 44100 Hz). Short indices decode to a length inside [kMinFrames,
// kMaxFrames]; mirrored here so the tests don't reach into the .cpp internals.
constexpr size_t kMinFrames = 100UL * DEFAULT_SAMPLE_RATE / 1000;  // 4410
constexpr size_t kMaxFrames = 5000UL * DEFAULT_SAMPLE_RATE / 1000; // 220500

} // namespace

TEST_CASE("Scramble: pure transform is an exact bijection", "[scramble][bijection]") {
    const uint64_t seed = 0xC0FFEE123456ULL;

    REQUIRE(IndexScramble::scramble(0, seed) == 0);
    REQUIRE(IndexScramble::unscramble(0, seed) == 0);

    std::mt19937_64 rng(2026);
    for (int t = 0; t < 200; ++t) {
        // Random non-negative integer of a random bit width.
        size_t  bits = rng() % 800;
        cpp_int n    = 0;
        for (size_t i = 0; i < (bits + 63) / 64; ++i) {
            n = (n << 64) | cpp_int(rng());
        }
        if (bits > 0) {
            n &= (cpp_int(1) << bits) - 1;
        }

        cpp_int s = IndexScramble::scramble(n, seed);
        INFO("n = " << n);
        REQUIRE(IndexScramble::unscramble(s, seed) == n);
    }
}

TEST_CASE("Scramble: neighbours are scattered, not adjacent", "[scramble][scatter]") {
    const uint64_t seed = 0x5EED;
    // Use indices large enough to have a wide band; n and n+1 differ only in the
    // low bits before scrambling and should differ widely after.
    cpp_int base("123456789012345678901234567890");
    cpp_int a = IndexScramble::scramble(base, seed);
    cpp_int b = IndexScramble::scramble(base + 1, seed);

    REQUIRE(a != b);
    cpp_int diff = (a > b) ? (a - b) : (b - a);
    // The difference should reach high into the band, not stay in the low bits.
    REQUIRE(boost::multiprecision::msb(diff) > 64);
}

TEST_CASE("Scramble: different seeds give different placements", "[scramble][seed]") {
    cpp_int n("987654321987654321");
    REQUIRE(IndexScramble::scramble(n, 1) != IndexScramble::scramble(n, 2));
}

TEST_CASE("Scramble: disabled config leaves the index untouched", "[scramble][toggle]") {
    std::vector<uint16_t> samples = {1, 2, 3, 0, 0, 65535, 4};
    auto                  bytes   = makePayload(samples);

    ScrambleGuard guard(false, 42); // explicitly disabled
    auto          rawIndex = Index::encode(bytes);

    // With scramble off, the index is the plain payload value (no permutation):
    // encoding again under the same disabled config reproduces it exactly.
    REQUIRE(Index::encode(bytes) == rawIndex);
    auto decoded = Index::decode(rawIndex);
    REQUIRE(decoded == bytes);
}

TEST_CASE("Scramble: enabled pipeline still round-trips exactly", "[scramble][toggle][roundtrip]") {
    const uint64_t seed = 0xABCDEF42ULL;

    // Capture the unscrambled index first for comparison.
    auto    bytes = makePayload({10, 20, 0, 0, 30, 65535, 0});
    cpp_int rawIndex;
    {
        ScrambleGuard off(false, seed);
        rawIndex = Index::encode(bytes);
    }

    ScrambleGuard on(true, seed);
    cpp_int       scrambledIndex = Index::encode(bytes);

    // Enabling the scramble must change the stored index...
    REQUIRE(scrambledIndex != rawIndex);

    // ...but the full round-trip must still reproduce the payload exactly.
    auto decoded = Index::decode(scrambledIndex);
    REQUIRE(decoded == bytes);
    REQUIRE(decoded.size() == bytes.size());
}

namespace {

// Recovers the sample-band L of an index, mirroring IndexScramble::bandIndex
// (which is file-local). L is the decoded sample count for that index.
auto bandOf(const cpp_int& n) -> size_t {
    if (n == 0) {
        return 0;
    }
    cpp_int m = (n * (SAMPLE_ALPHABET_SIZE - 1)) + 1;
    return static_cast<size_t>(boost::multiprecision::msb(m) / DEFAULT_BIT_DEPTH);
}

// S_L = (B^L - 1)/(B - 1), the exact start of sample-band L (mirrors the
// repunit used internally by IndexScramble and Index).
auto repunitOf(size_t band) -> cpp_int {
    if (band == 0) {
        return cpp_int(0);
    }
    return ((cpp_int(1) << (DEFAULT_BIT_DEPTH * band)) - 1) / (SAMPLE_ALPHABET_SIZE - 1);
}

// Builds an index that lands exactly in sample-band `band` (jitter must stay
// under the band width, true for any jitter fitting in a uint32_t here).
auto indexInBand(size_t band, uint32_t jitter) -> cpp_int {
    return repunitOf(band) + jitter;
}

} // namespace

TEST_CASE("Scramble: short indices spread across a wide, varied range of lengths", "[scramble][spread]") {
    const uint64_t seed = 0xD15EA5E5EEDULL;

    // The user-facing "type a short index, hear something interesting" path:
    // decoding runs unscramble() internally. Every short index must land within
    // the spread bounds (100 ms .. 15 s), and across a handful of them we should
    // see genuine variety, not one repeated length.
    ScrambleGuard    on(true, seed);
    std::set<size_t> distinctFrames;
    size_t           minSeen = kMaxFrames + 1;
    size_t           maxSeen = 0;
    for (cpp_int small : {cpp_int(1), cpp_int(2), cpp_int(7), cpp_int(42), cpp_int(12345), cpp_int(9600), cpp_int(19200), cpp_int("99999999")}) {
        auto   decoded = Index::decode(small);
        size_t frames  = decoded.size() / 2;
        INFO("short index = " << small << " -> frames = " << frames);
        REQUIRE(frames >= kMinFrames);
        REQUIRE(frames <= kMaxFrames);
        distinctFrames.insert(frames);
        minSeen = std::min(minSeen, frames);
        maxSeen = std::max(maxSeen, frames);
    }

    // Variety, not a single collapsed length (the old tier design produced one
    // length for every short index).
    REQUIRE(distinctFrames.size() >= 6);
    // The spread genuinely reaches both short and long ends of the range.
    REQUIRE(minSeen < DEFAULT_SAMPLE_RATE);     // at least one clip well under 1 s
    REQUIRE(maxSeen > 4 * DEFAULT_SAMPLE_RATE); // at least one clip over 4 s
}

TEST_CASE("Scramble: numerically adjacent short indices get different lengths", "[scramble][spread]") {
    const uint64_t seed = 0xBEEF;
    ScrambleGuard  on(true, seed);

    // Adjacent indices (as produced by stepping through library positions) must
    // not all decode to the same duration. Count how many consecutive pairs
    // differ in length across a small sweep.
    size_t prevFrames = 0;
    int    differing  = 0;
    for (int i = 1; i <= 32; ++i) {
        size_t frames = Index::decode(cpp_int(i)).size() / 2;
        if (i > 1 && frames != prevFrames) {
            ++differing;
        }
        prevFrames = frames;
    }
    // The vast majority of adjacent pairs should differ in length.
    REQUIRE(differing >= 24);
}

TEST_CASE("Scramble: the full set of target lengths is reachable", "[scramble][spread][bijection]") {
    const uint64_t seed = 0x1234ABCDULL;

    // Stepping through enough short indices should exercise the whole spread of
    // distinct target bands, confirming the length-selector is well distributed.
    std::set<size_t> bands;
    for (int i = 1; i <= 4000; ++i) {
        bands.insert(bandOf(IndexScramble::scramble(cpp_int(i), seed)));
    }
    // 256 distinct target lengths by default; we should see nearly all of them.
    REQUIRE(bands.size() >= 200);
}

TEST_CASE("Scramble: a 3-sample payload is still represented and exact", "[scramble][spread][roundtrip]") {
    const uint64_t seed = 0xA11CE;
    ScrambleGuard  on(true, seed);

    // The bijection still has room for tiny payloads; encoding one produces a
    // valid (scattered, much larger) index that decodes back to exactly 3
    // samples. Nothing about length-spreading removes short audio from the codomain.
    auto bytes = makePayload({1234, 0, 65535});
    auto index = Index::encode(bytes); // scrambled / "public" index
    auto back  = Index::decode(index);
    REQUIRE(back.size() == 6);
    REQUIRE(back == bytes);
}

TEST_CASE("Scramble: distinct short indices never collide", "[scramble][bijection][injective]") {
    // A true bijection cannot map two different inputs to the same output.
    const uint64_t    seed = 0x1357246ULL;
    std::set<cpp_int> outputs;
    for (uint32_t i = 1; i < 500; ++i) {
        cpp_int s = IndexScramble::scramble(cpp_int(i), seed);
        INFO("i = " << i);
        REQUIRE(outputs.insert(s).second); // false if s was already present
    }
}

TEST_CASE("Scramble: smallest payload lengths (0, 1, 2 samples) round-trip exactly", "[scramble][spread][roundtrip]") {
    const uint64_t seed = 0xFEED5EEDULL;
    ScrambleGuard  on(true, seed);

    REQUIRE(Index::decode(Index::encode(makePayload({}))).empty());

    for (auto samples : std::vector<std::vector<uint16_t>>{{42}, {0}, {65535}, {1, 2}, {0, 0}}) {
        auto bytes = makePayload(samples);
        auto index = Index::encode(bytes);
        auto back  = Index::decode(index);
        REQUIRE(back == bytes);
    }
}

TEST_CASE("Scramble: indices longer than the spread maximum keep exact length", "[scramble][spread][legacy]") {
    // The length-spread only moves SHORT indices. Anything already longer than
    // the spread's maximum target (661,500 samples / 15 s) keeps its band, so
    // its decoded length is preserved exactly — only its content is scattered.
    const uint64_t seed = 0xC0DEC0DEULL;
    size_t         L    = kMaxFrames + 5000; // comfortably past the spread maximum
    cpp_int        n    = indexInBand(L, 777);
    REQUIRE(bandOf(n) == L);

    cpp_int s = IndexScramble::scramble(n, seed);
    REQUIRE(bandOf(s) == L); // length-preserving for long inputs
    REQUIRE(IndexScramble::unscramble(s, seed) == n);
}

TEST_CASE("Scramble: a mid-length index between the targets keeps its length", "[scramble][spread][bijection]") {
    // An index whose band is neither short (spread prefix) nor an exact target
    // band passes through length-unchanged, with only its content scrambled.
    const uint64_t seed = 0xBADC0FFEE0DDF00DULL;
    size_t         L    = 1000; // 1000 samples: not a prefix, not a target band
    cpp_int        n    = indexInBand(L, 4242);
    REQUIRE(bandOf(n) == L);

    cpp_int s = IndexScramble::scramble(n, seed);
    REQUIRE(bandOf(s) == L);
    REQUIRE(IndexScramble::unscramble(s, seed) == n);
}

TEST_CASE("Scramble: invariants hold with scramble enabled", "[scramble][bijection][roundtrip]") {
    const uint64_t seed = 7;
    ScrambleGuard  on(true, seed);

    SECTION("Empty payload still maps to the empty index and back") {
        std::vector<uint8_t> empty{};
        auto                 idx = Index::encode(empty);
        REQUIRE(idx == 0);
        REQUIRE(Index::decode(idx).empty());
    }

    SECTION("Trailing-zero distinctness survives scrambling") {
        auto k0 = Index::encode(makePayload({5, 7, 0}));
        auto k1 = Index::encode(makePayload({5, 7, 0, 0}));
        REQUIRE(k0 != k1);
    }

    SECTION("Random payloads round-trip exactly") {
        std::mt19937                            rng(99);
        std::uniform_int_distribution<int>      lenDist(0, 80);
        std::uniform_int_distribution<uint32_t> valDist(0, 65535);
        for (int t = 0; t < 100; ++t) {
            std::vector<uint16_t> samples(static_cast<size_t>(lenDist(rng)));
            for (auto& v : samples) {
                v = static_cast<uint16_t>(valDist(rng));
            }
            auto bytes   = makePayload(samples);
            auto decoded = Index::decode(Index::encode(bytes));
            REQUIRE(decoded == bytes);
        }
    }
}
