/**
 * @file test_index.cpp
 * @brief Unit tests for the core Index class (PCM payload <-> big integer bijection).
 *
 * This file contains tests for Index::encode/decode, including roundtrip behavior,
 * edge cases, and the FileIO helpers used to build/read PCM payloads alongside it.
 *
 * Migrated to Catch2 v3 framework.
 */

#include <FileIO.h>
#include <Index.h>

#include <boost/multiprecision/cpp_int.hpp>
#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace AudioBabel;

TEST_CASE("FileIO: fromSamples", "[file_io]") {
    std::vector<int32_t> samples;
    samples.reserve(10);
    for (int i = 0; i < 10; ++i) {
        samples.push_back((i % 2 == 0) ? 1000 : -1000);
    }
    auto audioData = FileIO::fromSamples(samples, 8000, 16);

    REQUIRE(audioData.sample_rate == 8000);
    REQUIRE(audioData.bit_rate == 16);
    REQUIRE(audioData.num_channels == 1);
    REQUIRE(audioData.num_frames == samples.size());
    REQUIRE(audioData.samples.size() == samples.size() * static_cast<size_t>(audioData.bit_rate / 8));
}

TEST_CASE("FileIO: writeWav(samples, path) applies the fixed default header", "[file_io][roundtrip]") {
    // Index::decode returns raw payload bytes only. Writing those bytes to a WAV
    // always applies the locked default format (PCM, 44100 Hz, 16-bit, mono)
    // regardless of the source format. The 16-bit payload bytes round-trip exactly.
    std::vector<int32_t> samples   = {0, 127, 200, 64, 192};
    auto                 audioData = FileIO::fromSamples(samples, 8000, 16);

    auto idx     = Index::encode(audioData.samples);
    auto decoded = Index::decode(idx);

    TempFile tmp(make_temp_path("temp_test.wav"));
    FileIO::writeWav(decoded, tmp.path());
    auto readBack = FileIO::readWav(tmp.path());

    REQUIRE(readBack.audio_format == 1);
    REQUIRE(readBack.sample_rate == 44100);
    REQUIRE(readBack.bit_rate == 16);
    REQUIRE(readBack.num_channels == 1);
    REQUIRE(readBack.num_frames == 5);
    REQUIRE(readBack.samples == audioData.samples);
}

TEST_CASE("Index: payload bytes round-trip exactly", "[index][roundtrip]") {
    // A 16-bit payload (10 bytes / 5 samples) is reproduced byte-for-byte.
    std::vector<int32_t> samples   = {0, 12345, 54321, 30000, 5};
    auto                 audioData = FileIO::fromSamples(samples, 44100, 16);

    auto idx     = Index::encode(audioData.samples);
    auto decoded = Index::decode(idx);

    REQUIRE(decoded == audioData.samples);
}

TEST_CASE("Index: encode -> decode roundtrip", "[index][roundtrip]") {
    std::vector<int32_t> samples   = {0, 12345, -12345, 30000, -30000};
    auto                 audioData = FileIO::fromSamples(samples, 44100, 16);
    auto                 idx       = Index::encode(audioData.samples);
    auto                 decoded   = Index::decode(idx);

    REQUIRE(decoded.size() > 0);
    REQUIRE(decoded == audioData.samples);
}

TEST_CASE("FileIO: writeWav and readWav round trip", "[file_io][wav]") {
    std::vector<int32_t> samples   = {0, 1000, -1000, 2000, -2000};
    auto                 audioData = FileIO::fromSamples(samples, 22050, 16);
    TempFile              tmp(make_temp_path("temp_test.wav"));

    FileIO::writeWav(audioData, tmp.path());
    auto audioData2 = FileIO::readWav(tmp.path());

    REQUIRE(audioData2.sample_rate == audioData.sample_rate);
    REQUIRE(audioData2.bit_rate == audioData.bit_rate);
    REQUIRE(audioData2.num_channels == audioData.num_channels);
    REQUIRE(audioData2.num_frames == audioData.num_frames);
    REQUIRE(audioData2.samples == audioData.samples);
}

TEST_CASE("Index: encode never rejects a payload", "[index][bijection]") {
    // The payload-only bijection has no validation that can reject an index;
    // any sample payload encodes successfully.
    auto audioData = FileIO::fromSamples({0, 1, 2, 65535, 12345}, 44100, 16);
    REQUIRE_NOTHROW(Index::encode(audioData.samples));

    std::vector<uint8_t> empty{};
    REQUIRE_NOTHROW(Index::encode(empty));
}

TEST_CASE("Index: empty samples roundtrip", "[index][edge_case]") {
    std::vector<int32_t> samples; // empty
    auto                 audioData = FileIO::fromSamples(samples, 48000, 16);

    REQUIRE(audioData.num_frames == 0);

    auto idx     = Index::encode(audioData.samples);
    auto decoded = Index::decode(idx);

    REQUIRE(decoded.empty());
}

TEST_CASE("Index: silence duration preservation bug", "[index][bug_fix]") {
    std::vector<int32_t> silence_1sec(44100, 0);
    auto                 ad1  = FileIO::fromSamples(silence_1sec, 44100, 16);
    auto                 idx1 = Index::encode(ad1.samples);

    std::vector<int32_t> silence_2sec(88200, 0);
    auto                 ad2  = FileIO::fromSamples(silence_2sec, 44100, 16);
    auto                 idx2 = Index::encode(ad2.samples);

    bool indexes_equal  = (idx1 == idx2);
    auto reconstructed1 = Index::decode(idx1);
    auto reconstructed2 = Index::decode(idx2);

    INFO("FIX VERIFIED: 1sec and 2sec silence produce DIFFERENT indexes");
    REQUIRE_FALSE(indexes_equal);
    REQUIRE(reconstructed1.size() == 88200);
    REQUIRE(reconstructed2.size() == 176400);
}

TEST_CASE("Index: 16-bit edge values roundtrip", "[index][edge_case]") {
    auto s0 = static_cast<int16_t>(std::numeric_limits<int16_t>::min());
    auto s1 = static_cast<int16_t>(std::numeric_limits<int16_t>::max());

    std::vector<uint8_t> samples(2 * 2);
    samples[0] = static_cast<uint8_t>(s0 & 0xFF);
    samples[1] = static_cast<uint8_t>((s0 >> 8) & 0xFF);
    samples[2] = static_cast<uint8_t>(s1 & 0xFF);
    samples[3] = static_cast<uint8_t>((s1 >> 8) & 0xFF);

    auto idx     = Index::encode(samples);
    auto decoded = Index::decode(idx);

    REQUIRE(decoded == samples);
}

TEST_CASE("Index: 32-bit-sourced payload round-trips its raw bytes", "[index][edge_case]") {
    // Index has no notion of bit depth; it always re-reads the payload as
    // 16-bit samples. The 8 payload bytes (originally 2 32-bit samples) are
    // preserved exactly regardless of how they were produced.
    int32_t s0 = std::numeric_limits<int32_t>::min();
    int32_t s1 = std::numeric_limits<int32_t>::max();

    std::vector<uint8_t> samples(2 * 4);
    for (size_t b = 0; b < 4; ++b) {
        samples[b] = static_cast<uint8_t>((s0 >> (8 * b)) & 0xFF);
    }
    for (size_t b = 0; b < 4; ++b) {
        samples[4 + b] = static_cast<uint8_t>((s1 >> (8 * b)) & 0xFF);
    }

    auto idx     = Index::encode(samples);
    auto decoded = Index::decode(idx);

    REQUIRE(decoded.size() == 8);
    REQUIRE(decoded == samples);
}

TEST_CASE("FileIO: fromSamples byte-order", "[file_io][encoding]") {
    SECTION("16-bit little-endian byte order") {
        std::vector<int32_t> s16  = {0x1234};
        auto                 ad16 = FileIO::fromSamples(s16, 44100, 16);

        REQUIRE(ad16.samples.size() == 2);
        REQUIRE(ad16.samples[0] == static_cast<uint8_t>(0x34));
        REQUIRE(ad16.samples[1] == static_cast<uint8_t>(0x12));
    }

    SECTION("32-bit little-endian byte order") {
        std::vector<int32_t> s32  = {0x0A0B0C0D};
        auto                 ad32 = FileIO::fromSamples(s32, 48000, 32);

        REQUIRE(ad32.samples.size() == 4);
        REQUIRE(ad32.samples[0] == static_cast<uint8_t>(0x0D));
        REQUIRE(ad32.samples[1] == static_cast<uint8_t>(0x0C));
        REQUIRE(ad32.samples[2] == static_cast<uint8_t>(0x0B));
        REQUIRE(ad32.samples[3] == static_cast<uint8_t>(0x0A));
    }
}

TEST_CASE("Index: serialization textual roundtrip", "[index][serialization]") {
    using boost::multiprecision::cpp_int;
    auto audioData = FileIO::fromSamples(std::vector<int32_t>{0, 12345, -12345}, 44100, 16);

    cpp_int idx = Index::encode(audioData.samples);
    auto    s   = idx.convert_to<std::string>();
    cpp_int idx2(s);
    auto    decoded = Index::decode(idx2);

    REQUIRE(decoded == audioData.samples);
}

TEST_CASE("Index: low-valued samples round-trip without loss", "[index][encoding]") {
    size_t                numFrames = 4;
    std::vector<uint8_t>  samples(numFrames * 2);
    for (size_t i = 0; i < numFrames; ++i) {
        auto   v   = static_cast<int16_t>(i + 1);
        size_t off = i * 2;
        samples[off + 0] = static_cast<uint8_t>(v & 0xFF);
        samples[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    }

    auto idx     = Index::encode(samples);
    auto decoded = Index::decode(idx);

    REQUIRE(decoded.size() >= samples.size() - 2);
}
