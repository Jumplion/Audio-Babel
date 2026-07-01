/**
 * @file test_bijection.cpp
 * @brief Tests for the payload-only TRUE BIJECTION between PCM payloads,
 *        big-integer indices, and bijective base-64 index strings.
 *
 * Invariants covered:
 *  - Index::decode(Index::encode(x)) reproduces x exactly, including leading
 *    AND trailing zero (silence) samples and the exact count.
 *  - b64ToIndex(indexToB64(n)) == n for all n, and indexToB64(b64ToIndex(s)) == s.
 *  - k vs k+1 trailing zero samples yield different indices.
 *  - WAV (default format) -> index -> WAV reproduces the data chunk bytes exactly.
 */

#include <FileIO.h>
#include <Index.h>

#include <boost/multiprecision/cpp_int.hpp>
#include <catch2/catch_test_macros.hpp>
#include <random>

#include "test_common.h"

using namespace AudioBabel;
using boost::multiprecision::cpp_int;

namespace {

/// Round-trip a 16-bit sample payload through the index and return the decoded bytes.
std::vector<uint8_t> roundTripBytes(const std::vector<uint16_t>& samples) {
    auto bytes = makePayload(samples);
    auto idx   = Index::encode(bytes);
    return Index::decode(idx);
}

} // namespace

TEST_CASE("Bijection: payload round-trips exactly (edge cases)", "[bijection][roundtrip]") {
    SECTION("Empty payload <-> integer 0 <-> empty string") {
        std::vector<uint8_t> empty{};
        auto                 idx = Index::encode(empty);
        REQUIRE(idx == 0);
        REQUIRE(Utilities::indexToB64(idx).empty());

        auto decoded = Index::decode(idx);
        REQUIRE(decoded.empty());
    }

    // Table-driven replacement for what used to be six near-identical
    // single-case SECTIONs, each just checking roundTripBytes(s) == makePayload(s).
    std::vector<std::pair<std::string, std::vector<uint16_t>>> cases = {
        {"Single sample", {0x1234}},
        {"Single zero sample is preserved", {0}},
        {"Leading zero samples", {0, 0, 0, 42, 1000}},
        {"Trailing zero samples", {42, 1000, 0, 0, 0}},
        {"Leading and trailing zero samples", {0, 0, 7, 0, 65535, 0, 0}},
        {"Maximum-valued samples", {65535, 65535, 65535}},
    };

    for (const auto& [name, s] : cases) {
        INFO("Case: " << name);
        REQUIRE(roundTripBytes(s) == makePayload(s));
    }
}

TEST_CASE("Bijection: randomized round-trips (payload bytes and base64 strings)", "[bijection][roundtrip][random]") {
    SECTION("Random payloads of varying length round-trip through Index::encode/decode") {
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

            auto bytes   = makePayload(samples);
            auto idx     = Index::encode(bytes);
            auto decoded = Index::decode(idx);

            INFO("trial=" << trial << " len=" << len);
            REQUIRE(decoded == bytes);
            REQUIRE(decoded.size() == samples.size() * 2);
        }
    }

    SECTION("Random alphabet strings round-trip through indexToB64/b64ToIndex") {
        using Utilities::BASE64_URL_ALPHA;

        std::mt19937                       rng(987654321);
        std::uniform_int_distribution<int> lenDist(0, 40);
        std::uniform_int_distribution<int> chrDist(0, 63);

        for (int trial = 0; trial < 500; ++trial) {
            int         len = lenDist(rng);
            std::string original;
            original.reserve(len);
            for (int i = 0; i < len; ++i) {
                original.push_back(BASE64_URL_ALPHA[chrDist(rng)]);
            }

            // Every alphabet-valid string decodes without throwing.
            cpp_int     n = Utilities::b64ToIndex(original);
            std::string s = Utilities::indexToB64(n);

            INFO("trial=" << trial << " original='" << original << "'");
            REQUIRE(s == original);
        }
    }
}

TEST_CASE("Bijection: trailing-zero distinctness (N vs N+1)", "[bijection][distinctness]") {
    for (int k = 0; k < 8; ++k) {
        std::vector<uint16_t> base = {7, 11, 13};
        std::vector<uint16_t> withK(base);
        withK.insert(withK.end(), k, 0);
        std::vector<uint16_t> withKplus1(base);
        withKplus1.insert(withKplus1.end(), k + 1, 0);

        auto idxK     = Index::encode(makePayload(withK));
        auto idxKplus = Index::encode(makePayload(withKplus1));

        INFO("k=" << k);
        REQUIRE(idxK != idxKplus);
    }

    // Pure silence of different lengths must differ too.
    auto silence1 = Index::encode(makePayload(std::vector<uint16_t>(1, 0)));
    auto silence2 = Index::encode(makePayload(std::vector<uint16_t>(2, 0)));
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

    auto bytes   = makePayload(samples);
    auto idx     = Index::encode(bytes);
    auto decoded = Index::decode(idx);

    REQUIRE(decoded == bytes);
    REQUIRE(decoded.size() == N * 2);

    // The index string is itself a bijection and round-trips exactly.
    std::string s = Utilities::indexToB64(idx);
    REQUIRE(Utilities::b64ToIndex(s) == idx);
}

TEST_CASE("Bijection: WAV default-format data chunk round-trips exactly", "[bijection][wav][roundtrip]") {
    // Build a default-format payload, write a WAV, extract it, index it, decode,
    // and write a second WAV; the data-chunk bytes must match exactly.
    std::vector<uint16_t> samples = {0, 1, 2, 0, 0, 40000, 65535, 12345, 0};
    auto                  bytes   = makePayload(samples);

    TempFile srcWav(make_temp_path("bijection_src.wav"));
    FileIO::writeWav(bytes, srcWav.path());

    auto extracted = FileIO::readWav(srcWav.path());
    REQUIRE(extracted.samples == bytes);

    auto idx     = Index::encode(extracted.samples);
    auto decoded = Index::decode(idx);

    TempFile dstWav(make_temp_path("bijection_dst.wav"));
    FileIO::writeWav(decoded, dstWav.path());

    auto reExtracted = FileIO::readWav(dstWav.path());
    REQUIRE(reExtracted.samples == bytes);
    REQUIRE(reExtracted.sample_rate == DEFAULT_SAMPLE_RATE);
    REQUIRE(reExtracted.bit_rate == DEFAULT_BIT_DEPTH);
    REQUIRE(reExtracted.num_channels == DEFAULT_NUM_CHANNELS);
}
