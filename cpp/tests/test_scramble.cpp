/**
 * @file test_scramble.cpp
 * @brief Tests for the optional reversible index scramble.
 *
 * Covers the pure transform (bijection, inverse, 0->0, scatter) and the toggle
 * wired into the encode/decode pipeline (round-trip preserved, output differs
 * from the unscrambled index when enabled, core untouched when disabled).
 */

#include <AudioIndex.h>
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

/// Build an AudioData payload from unsigned 16-bit samples (little-endian).
AudioIndex::AudioData makePayload(const std::vector<uint16_t>& samples) {
    AudioIndex::AudioData ad{};
    ad.audio_format = 1;
    ad.sample_rate  = DEFAULT_SAMPLE_RATE;
    ad.bit_rate     = DEFAULT_BIT_DEPTH;
    ad.num_channels = DEFAULT_NUM_CHANNELS;
    ad.num_frames   = samples.size();
    for (uint16_t v : samples) {
        ad.samples.push_back(static_cast<uint8_t>(v & 0xFF));
        ad.samples.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }
    return ad;
}

} // namespace

TEST_CASE("Scramble: pure transform is an exact bijection", "[scramble][bijection]") {
    const uint64_t seed = 0xC0FFEE123456ULL;

    REQUIRE(IndexScramble::scramble(0, seed) == 0);
    REQUIRE(IndexScramble::unscramble(0, seed) == 0);

    // Each round-trip now permutes across a whole length-tier (the tier-1 domain
    // for these small values is ~88 KB), so this is intentionally a few hundred
    // iterations rather than thousands — still a thorough bijection fuzz.
    std::mt19937_64 rng(2026);
    for (int t = 0; t < 400; ++t) {
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
    auto                  ad      = makePayload(samples);

    ScrambleGuard guard(false, 42); // explicitly disabled
    auto          rawIndex = AudioIndex::audioDataToIndex(ad);

    // With scramble off, the index is the plain payload value (no permutation).
    REQUIRE(IndexScramble::applyScramble(rawIndex) == rawIndex);
    auto decoded = AudioIndex::indexToAudioData(rawIndex);
    REQUIRE(decoded.samples == ad.samples);
}

TEST_CASE("Scramble: enabled pipeline still round-trips exactly", "[scramble][toggle][roundtrip]") {
    const uint64_t seed = 0xABCDEF42ULL;

    // Capture the unscrambled index first for comparison.
    AudioIndex::AudioData ad = makePayload({10, 20, 0, 0, 30, 65535, 0});
    cpp_int               rawIndex;
    {
        ScrambleGuard off(false, seed);
        rawIndex = AudioIndex::audioDataToIndex(ad);
    }

    ScrambleGuard on(true, seed);
    cpp_int       scrambledIndex = AudioIndex::audioDataToIndex(ad);

    // Enabling the scramble must change the stored index...
    REQUIRE(scrambledIndex != rawIndex);

    // ...but the full round-trip must still reproduce the payload exactly.
    auto decoded = AudioIndex::indexToAudioData(scrambledIndex);
    REQUIRE(decoded.samples == ad.samples);
    REQUIRE(decoded.num_frames == ad.samples.size() / 2);
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
// repunit used internally by IndexScramble and AudioIndex).
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

TEST_CASE("Scramble: short indices spread into a much longer length tier", "[scramble][tier]") {
    const uint64_t seed = 0xD15EA5E5EEDULL;

    // A handful of short indices (a few samples each) of the kind a user might
    // type. Decoding runs unscramble() internally, so this is the user-facing
    // "type a short index, hear something interesting" path. Tier 1 caps at
    // 44100 samples (1s @ 44.1 kHz); the top band holds ~(1 - 1/B) of the tier,
    // so each one should land near that cap rather than at a few samples.
    ScrambleGuard on(true, seed);
    for (cpp_int small : {cpp_int(1), cpp_int(7), cpp_int(12345), cpp_int("99999999")}) {
        auto decoded = AudioIndex::indexToAudioData(small);
        INFO("short index = " << small << " -> frames = " << decoded.num_frames);
        REQUIRE(decoded.num_frames >= 40000); // within the tier-1 (1s) cap
        REQUIRE(decoded.num_frames <= 44100);
    }
}

TEST_CASE("Scramble: a 3-sample payload is still represented and exact", "[scramble][tier][roundtrip]") {
    const uint64_t seed = 0xA11CE;
    ScrambleGuard  on(true, seed);

    // The bijection still has room for tiny payloads; encoding one produces a
    // valid (scattered, much larger) index that decodes back to exactly 3
    // samples. Nothing about tiering removes short audio from the codomain.
    auto ad    = makePayload({1234, 0, 65535});
    auto index = AudioIndex::audioDataToIndex(ad); // scrambled / "public" index
    auto back  = AudioIndex::indexToAudioData(index);
    REQUIRE(back.num_frames == 3);
    REQUIRE(back.samples == ad.samples);
}

TEST_CASE("Scramble: tiered permutation is a bijection and stays within its tier", "[scramble][tier][bijection]") {
    const uint64_t seed = 0xBADC0FFEE0DDF00DULL;

    // One representative band inside several tiers: tier 2 (<=5s), tier 4
    // (<=20s) and tier 6 (<=45s). scramble()/unscramble() must round-trip, and
    // the scrambled index must stay inside the same tier's length bounds.
    struct Case {
        size_t band;
        size_t tierLow;
        size_t tierHigh;
    };
    const std::vector<Case> cases = {
        {150000, 44101, 220500},    // tier 2
        {700000, 441001, 882000},   // tier 4
        {1500000, 1323001, 1984500} // tier 6
    };

    for (const auto& c : cases) {
        cpp_int n = indexInBand(c.band, 4242);
        INFO("band ~= " << c.band);
        REQUIRE(bandOf(n) >= c.tierLow);
        REQUIRE(bandOf(n) <= c.tierHigh);

        cpp_int s = IndexScramble::scramble(n, seed);
        REQUIRE(IndexScramble::unscramble(s, seed) == n);

        // Length changed (scattered) but is still bounded by the same tier.
        size_t sBand = bandOf(s);
        REQUIRE(sBand >= c.tierLow);
        REQUIRE(sBand <= c.tierHigh);
    }
}

TEST_CASE("Scramble: distinct indices in a tier never collide", "[scramble][tier][bijection][injective]") {
    // A true bijection cannot map two different inputs to the same output.
    // Sample many distinct indices from the same band (well within tier 1) and
    // confirm every scrambled output is unique. With a domain of ~2^(16*44100)
    // values, any observed collision among a few thousand draws would indicate
    // a real bug, not bad luck.
    const uint64_t    seed = 0x1357246ULL;
    std::set<cpp_int> outputs;
    for (uint32_t i = 0; i < 1500; ++i) {
        cpp_int n = indexInBand(10, i); // band well inside tier 1
        cpp_int s = IndexScramble::scramble(n, seed);
        INFO("i = " << i);
        REQUIRE(outputs.insert(s).second); // false if s was already present
    }
}

TEST_CASE("Scramble: smallest payload lengths (0, 1, 2 samples) round-trip exactly", "[scramble][tier][roundtrip]") {
    const uint64_t seed = 0xFEED5EEDULL;
    ScrambleGuard  on(true, seed);

    REQUIRE(AudioIndex::indexToAudioData(AudioIndex::audioDataToIndex(makePayload({}))).samples.empty());

    for (auto samples : std::vector<std::vector<uint16_t>>{{42}, {0}, {65535}, {1, 2}, {0, 0}}) {
        auto ad    = makePayload(samples);
        auto index = AudioIndex::audioDataToIndex(ad);
        auto back  = AudioIndex::indexToAudioData(index);
        REQUIRE(back.samples == ad.samples);
    }
}

TEST_CASE("Scramble: payloads beyond the last tier keep exact length", "[scramble][tier][legacy]") {
    // Tier 11 caps at 10,584,000 samples (240s). Anything longer keeps the
    // original per-band permutation, so length must be preserved exactly, just
    // like the un-tiered scramble did before tiering existed.
    const uint64_t seed = 0xC0DEC0DEULL;
    size_t         L    = 10584000 + 50; // just past the last tier
    cpp_int        n    = indexInBand(L, 777);
    REQUIRE(bandOf(n) == L);

    cpp_int s = IndexScramble::scramble(n, seed);
    REQUIRE(bandOf(s) == L); // length-preserving, as before tiering
    REQUIRE(IndexScramble::unscramble(s, seed) == n);
}

TEST_CASE("Scramble: invariants hold with scramble enabled", "[scramble][bijection][roundtrip]") {
    const uint64_t seed = 7;
    ScrambleGuard  on(true, seed);

    SECTION("Empty payload still maps to the empty index and back") {
        AudioIndex::AudioData empty{};
        auto                  idx = AudioIndex::audioDataToIndex(empty);
        REQUIRE(idx == 0);
        REQUIRE(AudioIndex::indexToAudioData(idx).samples.empty());
    }

    SECTION("Trailing-zero distinctness survives scrambling") {
        auto k0 = AudioIndex::audioDataToIndex(makePayload({5, 7, 0}));
        auto k1 = AudioIndex::audioDataToIndex(makePayload({5, 7, 0, 0}));
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
            auto ad      = makePayload(samples);
            auto decoded = AudioIndex::indexToAudioData(AudioIndex::audioDataToIndex(ad));
            REQUIRE(decoded.samples == ad.samples);
        }
    }
}
