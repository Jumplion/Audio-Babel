/**
 * @file test_audio_index.cpp
 * @brief Unit tests for the core AudioIndex class.
 *
 * This file contains tests for the main functionalities of the AudioIndex class,
 * including serialization, deserialization (roundtrip), operator overloads,
 * and various edge cases.
 * 
 * Migrated to Catch2 v3 framework.
 */

#include <AudioIndex.h>

#include <boost/multiprecision/cpp_int.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "test_common.h"

using namespace AudioBabel;
using Catch::Matchers::WithinAbs;

TEST_CASE("AudioIndex: extractAudioDataFromSamples", "[audio_index]") {
    std::vector<int32_t> samples;
    samples.reserve(10);
    for (int i = 0; i < 10; ++i) {
        samples.push_back((i % 2 == 0) ? 1000 : -1000);
    }
    auto audioData = AudioIndex::extractAudioDataFromSamples(samples, 8000, 16);

    REQUIRE(audioData.sample_rate == 8000);
    REQUIRE(audioData.bit_rate == 16);
    REQUIRE(audioData.num_channels == 1);
    REQUIRE(audioData.num_frames == samples.size());
    REQUIRE(audioData.samples.size() == samples.size() * static_cast<size_t>(audioData.bit_rate / 8));
}

TEST_CASE("AudioIndex: audioData -> index -> audioData roundtrip (8-bit)", "[audio_index][roundtrip]") {
    std::vector<int32_t> samples   = {0, 127, -128, 64, -64};
    auto                 audioData = AudioIndex::extractAudioDataFromSamples(samples, 8000, 8);

    auto idx        = AudioIndex::audioDataToIndex(audioData);
    auto audioData2 = AudioIndex::indexToAudioData(idx);

    REQUIRE(audioData2.sample_rate == 8000);
    REQUIRE(audioData2.bit_rate == 8);
    REQUIRE(audioData2.num_channels == 1);
    REQUIRE(audioData2.num_frames == 5);
    REQUIRE(audioData2.samples.size() == 5);
}

TEST_CASE("AudioIndex: audioData -> index -> audioData roundtrip (32-bit)", "[audio_index][roundtrip]") {
    std::vector<int32_t> samples   = {0, 2147483647, -2147483647, 1000000, -1000000};
    auto                 audioData = AudioIndex::extractAudioDataFromSamples(samples, 48000, 32);

    auto idx        = AudioIndex::audioDataToIndex(audioData);
    auto audioData2 = AudioIndex::indexToAudioData(idx);

    REQUIRE(audioData2.sample_rate == 48000);
    REQUIRE(audioData2.bit_rate == 32);
    REQUIRE(audioData2.num_channels == 1);
    REQUIRE(audioData2.num_frames == 5);
    REQUIRE(audioData2.samples.size() == 20);
}

TEST_CASE("AudioIndex: audioData -> index -> audioData roundtrip", "[audio_index][roundtrip]") {
    std::vector<int32_t> samples    = {0, 12345, -12345, 30000, -30000};
    auto                 audioData  = AudioIndex::extractAudioDataFromSamples(samples, 44100, 16);
    auto                 idx        = AudioIndex::audioDataToIndex(audioData);
    auto                 audioData2 = AudioIndex::indexToAudioData(idx);

    REQUIRE(audioData2.sample_rate == 44100);
    REQUIRE(audioData2.bit_rate == 16);
    REQUIRE(audioData2.num_channels == 1);
    REQUIRE(audioData2.num_frames >= audioData.num_frames - 1);
    REQUIRE(audioData2.num_frames <= audioData.num_frames + 1);
    REQUIRE(audioData2.samples.size() > 0);
}

TEST_CASE("AudioIndex: exportAudioDataToWav and read back", "[audio_index][wav]") {
    std::vector<int32_t> samples   = {0, 1000, -1000, 2000, -2000};
    auto                 audioData = AudioIndex::extractAudioDataFromSamples(samples, 22050, 16);
    TempFile             tmp(make_temp_path("temp_test.wav"));

    AudioIndex::exportAudioDataToWav(audioData, tmp.path());
    auto audioData2 = AudioIndex::extractAudioDataFromAudioFile(tmp.path());

    REQUIRE(audioData2.sample_rate == audioData.sample_rate);
    REQUIRE(audioData2.bit_rate == audioData.bit_rate);
    REQUIRE(audioData2.num_channels == audioData.num_channels);
    REQUIRE(audioData2.num_frames == audioData.num_frames);
    REQUIRE(audioData2.samples == audioData.samples);
}

TEST_CASE("AudioIndex: unsupported bit depth throws", "[audio_index][error]") {
    std::vector<int32_t> samples   = {0, 1, 2};
    auto                 audioData = AudioIndex::extractAudioDataFromSamples(samples, 8000, 12); // 12-bit unsupported

    REQUIRE_THROWS(AudioIndex::audioDataToIndex(audioData));
}

TEST_CASE("AudioIndex: empty samples roundtrip", "[audio_index][edge_case]") {
    std::vector<int32_t> samples; // empty
    auto                 audioData = AudioIndex::extractAudioDataFromSamples(samples, 48000, 16);

    REQUIRE(audioData.num_frames == 0);

    auto idx        = AudioIndex::audioDataToIndex(audioData);
    auto audioData2 = AudioIndex::indexToAudioData(idx);

    REQUIRE(audioData2.num_frames == 0);
    REQUIRE(audioData2.samples.empty());
}

TEST_CASE("AudioIndex: silence duration preservation bug", "[audio_index][bug_fix]") {
    std::vector<int32_t> silence_1sec(44100, 0);
    auto                 ad1  = AudioIndex::extractAudioDataFromSamples(silence_1sec, 44100, 16);
    auto                 idx1 = AudioIndex::audioDataToIndex(ad1);

    std::vector<int32_t> silence_2sec(88200, 0);
    auto                 ad2  = AudioIndex::extractAudioDataFromSamples(silence_2sec, 44100, 16);
    auto                 idx2 = AudioIndex::audioDataToIndex(ad2);

    bool indexes_equal  = (idx1 == idx2);
    auto reconstructed1 = AudioIndex::indexToAudioData(idx1);
    auto reconstructed2 = AudioIndex::indexToAudioData(idx2);

    INFO("FIX VERIFIED: 1sec and 2sec silence produce DIFFERENT indexes");
    REQUIRE_FALSE(indexes_equal);
    REQUIRE(reconstructed1.num_frames == 44100);
    REQUIRE(reconstructed2.num_frames == 88200);
    REQUIRE(reconstructed1.samples.size() == 88200);
    REQUIRE(reconstructed2.samples.size() == 176400);
}

TEST_CASE("AudioIndex: zero sampleRate duration is zero", "[audio_index][edge_case]") {
    std::vector<int32_t> samples(10, 1000);
    auto                 ai = AudioIndex::fromAudioSamples(samples, 0, 16);

    REQUIRE_THAT(ai.getDuration(), WithinAbs(0.0, 1e-12));
}

TEST_CASE("AudioIndex: operator== equal objects", "[audio_index][operators]") {
    std::vector<int32_t> samples = {100, -100, 200, -200};
    auto                 a       = AudioIndex::fromAudioSamples(samples, 44100, 16);
    auto                 b       = AudioIndex::fromAudioSamples(samples, 44100, 16);

    REQUIRE(a == b);
    REQUIRE_FALSE(a != b);
}

TEST_CASE("AudioIndex: operator== different samples unequal", "[audio_index][operators]") {
    std::vector<int32_t> s1 = {0, 1, 2, 3};
    std::vector<int32_t> s2 = {0, 1, 2, 4};
    auto                 a  = AudioIndex::fromAudioSamples(s1, 44100, 16);
    auto                 b  = AudioIndex::fromAudioSamples(s2, 44100, 16);

    REQUIRE(a != b);
    REQUIRE_FALSE(a == b);
}

TEST_CASE("AudioIndex: operator== different sampleRate unequal", "[audio_index][operators]") {
    std::vector<int32_t> samples = {10, 20, 30, 40};
    auto                 a       = AudioIndex::fromAudioSamples(samples, 44100, 16);
    auto                 b       = AudioIndex::fromAudioSamples(samples, 22050, 16);

    REQUIRE(a != b);
    REQUIRE_FALSE(a == b);
}

TEST_CASE("AudioIndex: 16-bit edge values roundtrip", "[audio_index][edge_case]") {
    AudioIndex::AudioData audioData{};
    audioData.sample_rate  = 44100;
    audioData.bit_rate     = 16;
    audioData.num_channels = 1;
    audioData.audio_format = 1;
    audioData.num_frames   = 2;

    auto s0 = static_cast<int16_t>(std::numeric_limits<int16_t>::min());
    auto s1 = static_cast<int16_t>(std::numeric_limits<int16_t>::max());
    audioData.samples.resize(2 * 2);
    audioData.samples[0] = static_cast<uint8_t>(s0 & 0xFF);
    audioData.samples[1] = static_cast<uint8_t>((s0 >> 8) & 0xFF);
    audioData.samples[2] = static_cast<uint8_t>(s1 & 0xFF);
    audioData.samples[3] = static_cast<uint8_t>((s1 >> 8) & 0xFF);

    auto idx        = AudioIndex::audioDataToIndex(audioData);
    auto audioData2 = AudioIndex::indexToAudioData(idx);

    REQUIRE(audioData2.bit_rate == audioData.bit_rate);
    REQUIRE(audioData2.num_frames == audioData.num_frames);
    REQUIRE(audioData2.samples == audioData.samples);
}

TEST_CASE("AudioIndex: 32-bit edge values roundtrip", "[audio_index][edge_case]") {
    AudioIndex::AudioData audioData{};
    audioData.sample_rate  = 48000;
    audioData.bit_rate     = 32;
    audioData.num_channels = 1;
    audioData.audio_format = 1;
    audioData.num_frames   = 2;
    int32_t s0             = std::numeric_limits<int32_t>::min();
    int32_t s1             = std::numeric_limits<int32_t>::max();
    audioData.samples.resize(2 * 4);
    for (size_t b = 0; b < 4; ++b) {
        audioData.samples[b] = static_cast<uint8_t>((s0 >> (8 * b)) & 0xFF);
    }
    for (size_t b = 0; b < 4; ++b) {
        audioData.samples[4 + b] = static_cast<uint8_t>((s1 >> (8 * b)) & 0xFF);
    }

    auto idx        = AudioIndex::audioDataToIndex(audioData);
    auto audioData2 = AudioIndex::indexToAudioData(idx);

    REQUIRE(audioData2.bit_rate == 32);
    REQUIRE(audioData2.sample_rate == 48000);
    REQUIRE(audioData2.num_frames == 2);
    REQUIRE(audioData2.samples.size() == 8);
    REQUIRE(audioData2.samples == audioData.samples);
}

TEST_CASE("AudioIndex: extractAudioDataFromSamples byte-order", "[audio_index][encoding]") {
    SECTION("16-bit little-endian byte order") {
        std::vector<int32_t> s16  = {0x1234};
        auto                 ad16 = AudioIndex::extractAudioDataFromSamples(s16, 44100, 16);

        REQUIRE(ad16.samples.size() == 2);
        REQUIRE(ad16.samples[0] == static_cast<uint8_t>(0x34));
        REQUIRE(ad16.samples[1] == static_cast<uint8_t>(0x12));
    }

    SECTION("32-bit little-endian byte order") {
        std::vector<int32_t> s32  = {0x0A0B0C0D};
        auto                 ad32 = AudioIndex::extractAudioDataFromSamples(s32, 48000, 32);

        REQUIRE(ad32.samples.size() == 4);
        REQUIRE(ad32.samples[0] == static_cast<uint8_t>(0x0D));
        REQUIRE(ad32.samples[1] == static_cast<uint8_t>(0x0C));
        REQUIRE(ad32.samples[2] == static_cast<uint8_t>(0x0B));
        REQUIRE(ad32.samples[3] == static_cast<uint8_t>(0x0A));
    }
}

TEST_CASE("AudioIndex: serialization textual roundtrip", "[audio_index][serialization]") {
    using boost::multiprecision::cpp_int;
    AudioIndex::AudioData audioData = AudioIndex::extractAudioDataFromSamples(std::vector<int32_t>{0, 12345, -12345}, 44100, 16);

    cpp_int idx = AudioIndex::audioDataToIndex(audioData);
    auto    s   = idx.convert_to<std::string>();
    cpp_int idx2(s);
    auto    audioData2 = AudioIndex::indexToAudioData(idx2);

    REQUIRE(audioData2.sample_rate == audioData.sample_rate);
    REQUIRE(audioData2.bit_rate == audioData.bit_rate);
    REQUIRE(audioData2.num_frames == audioData.num_frames);
    REQUIRE(audioData2.samples == audioData.samples);
}

TEST_CASE("AudioIndex: export_bits padding behavior", "[audio_index][encoding]") {
    AudioIndex::AudioData audioData{};
    audioData.sample_rate  = 44100;
    audioData.bit_rate     = 16;
    audioData.num_channels = 1;
    audioData.audio_format = 1;
    audioData.num_frames   = 4;
    size_t bytes           = audioData.num_frames * audioData.num_channels * (audioData.bit_rate / 8);
    audioData.samples.resize(bytes);
    for (size_t i = 0; i < audioData.num_frames; ++i) {
        auto   v                   = static_cast<int16_t>(i + 1);
        size_t off                 = i * 2;
        audioData.samples[off + 0] = static_cast<uint8_t>(v & 0xFF);
        audioData.samples[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    }

    AudioIndex::clearLastDebugInfo();

    auto idx        = AudioIndex::audioDataToIndex(audioData);
    auto audioData2 = AudioIndex::indexToAudioData(idx);

    REQUIRE(audioData2.bit_rate == 16);
    REQUIRE(audioData2.sample_rate == 44100);
    REQUIRE(audioData2.samples.size() >= audioData.samples.size() - 2);
}