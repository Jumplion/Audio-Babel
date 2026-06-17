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

    std::mt19937_64 rng(2026);
    for (int t = 0; t < 5000; ++t) {
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
