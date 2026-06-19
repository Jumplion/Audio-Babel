/**
 * @file test_bijection.cpp
 * @brief Tests for the payload-only TRUE BIJECTION between PCM payloads,
 *        big-integer indices, and bijective base-64 index strings.
 *
 * Invariants covered:
 *  - indexToAudioData(audioDataToIndex(x)) reproduces x's samples exactly,
 *    including leading AND trailing zero (silence) samples and the exact count.
 *  - b64ToIndex(indexToB64(n)) == n for all n, and indexToB64(b64ToIndex(s)) == s.
 *  - k vs k+1 trailing zero samples yield different indices.
 *  - WAV (default format) -> index -> WAV reproduces the data chunk bytes exactly.
 */

#include <AudioIndex.h>

#include <boost/multiprecision/cpp_int.hpp>
#include <catch2/catch_test_macros.hpp>
#include <random>

#include "test_common.h"

using namespace AudioBabel;
using boost::multiprecision::cpp_int;

namespace {

/// Build an AudioData payload from a list of unsigned 16-bit samples (little-endian).
AudioIndex::AudioData makePayload(const std::vector<uint16_t>& samples) {
    AudioIndex::AudioData ad{};
    ad.audio_format = 1;
    ad.sample_rate  = DEFAULT_SAMPLE_RATE;
    ad.bit_rate     = DEFAULT_BIT_DEPTH;
    ad.num_channels = DEFAULT_NUM_CHANNELS;
    ad.num_frames   = samples.size();
    ad.samples.reserve(samples.size() * 2);
    for (uint16_t v : samples) {
        ad.samples.push_back(static_cast<uint8_t>(v & 0xFF));
        ad.samples.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }
    return ad;
}

/// Round-trip a 16-bit sample payload through the index and return the decoded bytes.
std::vector<uint8_t> roundTripBytes(const std::vector<uint16_t>& samples) {
    auto ad  = makePayload(samples);
    auto idx = AudioIndex::audioDataToIndex(ad);
    return AudioIndex::indexToAudioData(idx).samples;
}

} // namespace

TEST_CASE("Bijection: payload round-trips exactly (edge cases)", "[bijection][roundtrip]") {
    SECTION("Empty payload <-> integer 0 <-> empty string") {
        AudioIndex::AudioData empty{};
        auto                  idx = AudioIndex::audioDataToIndex(empty);
        REQUIRE(idx == 0);
        REQUIRE(Utilities::indexToB64(idx).empty());

        auto decoded = AudioIndex::indexToAudioData(idx);
        REQUIRE(decoded.samples.empty());
        REQUIRE(decoded.num_frames == 0);
    }

    SECTION("Single sample") {
        std::vector<uint16_t> s = {0x1234};
        REQUIRE(roundTripBytes(s) == makePayload(s).samples);
    }

    SECTION("Single zero sample is preserved") {
        std::vector<uint16_t> s       = {0};
        auto                  decoded = roundTripBytes(s);
        REQUIRE(decoded.size() == 2);
        REQUIRE(decoded == makePayload(s).samples);
    }

    SECTION("Leading zero samples") {
        std::vector<uint16_t> s = {0, 0, 0, 42, 1000};
        REQUIRE(roundTripBytes(s) == makePayload(s).samples);
    }

    SECTION("Trailing zero samples") {
        std::vector<uint16_t> s = {42, 1000, 0, 0, 0};
        REQUIRE(roundTripBytes(s) == makePayload(s).samples);
    }

    SECTION("Leading and trailing zero samples") {
        std::vector<uint16_t> s = {0, 0, 7, 0, 65535, 0, 0};
        REQUIRE(roundTripBytes(s) == makePayload(s).samples);
    }

    SECTION("Maximum-valued samples") {
        std::vector<uint16_t> s = {65535, 65535, 65535};
        REQUIRE(roundTripBytes(s) == makePayload(s).samples);
    }
}

TEST_CASE("Bijection: random payloads of varying length round-trip", "[bijection][roundtrip][random]") {
    std::mt19937                            rng(20260617);
    std::uniform_int_distribution<int>      lenDist(0, 200);
    std::uniform_int_distribution<uint32_t> valDist(0, 65535);

    for (int trial = 0; trial < 200; ++trial) {
        size_t                len = static_cast<size_t>(lenDist(rng));
        std::vector<uint16_t> samples;
        samples.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            samples.push_back(static_cast<uint16_t>(valDist(rng)));
        }

        auto ad      = makePayload(samples);
        auto idx     = AudioIndex::audioDataToIndex(ad);
        auto decoded = AudioIndex::indexToAudioData(idx);

        INFO("trial=" << trial << " len=" << len);
        REQUIRE(decoded.samples == ad.samples);
        REQUIRE(decoded.num_frames == samples.size());
    }
}

TEST_CASE("Bijection: trailing-zero distinctness (N vs N+1)", "[bijection][distinctness]") {
    for (int k = 0; k < 8; ++k) {
        std::vector<uint16_t> base = {7, 11, 13};
        std::vector<uint16_t> withK(base);
        withK.insert(withK.end(), k, 0);
        std::vector<uint16_t> withKplus1(base);
        withKplus1.insert(withKplus1.end(), k + 1, 0);

        auto idxK     = AudioIndex::audioDataToIndex(makePayload(withK));
        auto idxKplus = AudioIndex::audioDataToIndex(makePayload(withKplus1));

        INFO("k=" << k);
        REQUIRE(idxK != idxKplus);
    }

    // Pure silence of different lengths must differ too.
    auto silence1 = AudioIndex::audioDataToIndex(makePayload(std::vector<uint16_t>(1, 0)));
    auto silence2 = AudioIndex::audioDataToIndex(makePayload(std::vector<uint16_t>(2, 0)));
    REQUIRE(silence1 != silence2);
}

TEST_CASE("Bijection: integer -> string -> integer", "[bijection][string]") {
    std::vector<cpp_int> values = {
        0, 1, 2, 63, 64, 65, 4095, 4096, 65535, 65536, cpp_int("18446744073709551616"), cpp_int("123456789012345678901234567890")};

    for (const auto& n : values) {
        std::string s    = Utilities::indexToB64(n);
        cpp_int     back = Utilities::b64ToIndex(s);
        INFO("n=" << n << " s='" << s << "'");
        REQUIRE(back == n);
    }
}

TEST_CASE("Bijection: random alphabet strings -> integer -> string", "[bijection][string][random]") {
    static const std::string ALPHA = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::mt19937                       rng(987654321);
    std::uniform_int_distribution<int> lenDist(0, 40);
    std::uniform_int_distribution<int> chrDist(0, 63);

    for (int trial = 0; trial < 500; ++trial) {
        int         len = lenDist(rng);
        std::string original;
        original.reserve(len);
        for (int i = 0; i < len; ++i) {
            original.push_back(ALPHA[chrDist(rng)]);
        }

        // Every alphabet-valid string decodes without throwing.
        cpp_int     n = Utilities::b64ToIndex(original);
        std::string s = Utilities::indexToB64(n);

        INFO("trial=" << trial << " original='" << original << "'");
        REQUIRE(s == original);
    }
}

TEST_CASE("Bijection: large payload round-trips exactly (O(N) path)", "[bijection][roundtrip][large]") {
    // A large payload exercises the closed-form O(N) conversion. Under the old
    // per-sample O(L^2) loop this size would take minutes; here it is instant.
    // We mix leading zeros, trailing zeros, and extreme values to stress carries.
    std::mt19937                            rng(424242);
    std::uniform_int_distribution<uint32_t> valDist(0, 65535);

    const size_t          N = 300000;
    std::vector<uint16_t> samples(N, 0);
    for (size_t i = 1000; i + 1000 < N; ++i) {
        samples[i] = static_cast<uint16_t>(valDist(rng));
    }
    samples[N / 2] = 65535;

    auto ad      = makePayload(samples);
    auto idx     = AudioIndex::audioDataToIndex(ad);
    auto decoded = AudioIndex::indexToAudioData(idx);

    REQUIRE(decoded.samples == ad.samples);
    REQUIRE(decoded.num_frames == N);

    // The index string is itself a bijection and round-trips exactly.
    std::string s = Utilities::indexToB64(idx);
    REQUIRE(Utilities::b64ToIndex(s) == idx);
}

TEST_CASE("Bijection: WAV default-format data chunk round-trips exactly", "[bijection][wav][roundtrip]") {
    // Build a default-format payload, write a WAV, extract it, index it, decode,
    // and write a second WAV; the data-chunk bytes must match exactly.
    std::vector<uint16_t> samples = {0, 1, 2, 0, 0, 40000, 65535, 12345, 0};
    auto                  ad      = makePayload(samples);

    TempFile srcWav(make_temp_path("bijection_src.wav"));
    FileWriters::exportAudioDataToWav(ad, srcWav.path());

    auto extracted = AudioIndex::extractAudioDataFromAudioFile(srcWav.path());
    REQUIRE(extracted.samples == ad.samples);

    auto idx     = AudioIndex::audioDataToIndex(extracted);
    auto decoded = AudioIndex::indexToAudioData(idx);

    TempFile dstWav(make_temp_path("bijection_dst.wav"));
    FileWriters::exportAudioDataToWav(decoded, dstWav.path());

    auto reExtracted = AudioIndex::extractAudioDataFromAudioFile(dstWav.path());
    REQUIRE(reExtracted.samples == ad.samples);
    REQUIRE(reExtracted.sample_rate == DEFAULT_SAMPLE_RATE);
    REQUIRE(reExtracted.bit_rate == DEFAULT_BIT_DEPTH);
    REQUIRE(reExtracted.num_channels == DEFAULT_NUM_CHANNELS);
}
