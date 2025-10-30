/**
 * @file test_audio_index.cpp
 * @brief Unit tests for the core AudioIndex class.
 *
 * This file contains tests for the main functionalities of the AudioIndex class,
 * including serialization, deserialization (roundtrip), operator overloads,
 * and various edge cases.
 */

#include "test_common.h"
#include <AudioIndex.h>
#include <boost/multiprecision/cpp_int.hpp>

void register_audio_index_tests(TestRunner& runner) {
    runner.add("AudioIndex: extractAudioDataFromSamples", [&runner]() -> bool {
        const std::string    name = "AudioIndex: extractAudioDataFromSamples";
        std::vector<int32_t> samples;
        samples.reserve(10);
        for (int i = 0; i < 10; ++i) {
            samples.push_back((i % 2 == 0) ? 1000 : -1000);
        }
        auto audioData = AudioIndex::extractAudioDataFromSamples(samples, 8000, 16);

        bool ok = true;
        ok &= RUN_CHECK(runner, name, audioData.sample_rate == 8000, "sample_rate");
        ok &= RUN_CHECK(runner, name, audioData.bit_rate == 16, "bit_rate");
        ok &= RUN_CHECK(runner, name, audioData.num_channels == 1, "num_channels");
        ok &= RUN_CHECK(runner, name, audioData.num_frames == samples.size(), "num_frames");
        ok &=
            RUN_CHECK(runner, name, audioData.samples.size() == samples.size() * static_cast<size_t>(audioData.bit_rate / 8), "samples byte length");
        return ok;
    });

    runner.add("AudioIndex: audioData -> index -> audioData roundtrip (8-bit)", [&runner]() -> bool {
        const std::string    name      = "AudioIndex: audioData -> index -> audioData roundtrip (8-bit)";
        std::vector<int32_t> samples   = {0, 127, -128, 64, -64};
        auto                 audioData = AudioIndex::extractAudioDataFromSamples(samples, 8000, 8);
        try {
            auto idx        = AudioIndex::audioDataToIndex(audioData);
            auto audioData2 = AudioIndex::indexToAudioData(idx);
            bool ok         = true;
            ok &= RUN_CHECK(runner, name, audioData2.sample_rate == 8000, "sample_rate matches (8000)");
            ok &= RUN_CHECK(runner, name, audioData2.bit_rate == 8, "bit_rate matches (8-bit)");
            ok &= RUN_CHECK(runner, name, audioData2.num_channels == 1, "num_channels is 1");
            ok &= RUN_CHECK(runner, name, audioData2.num_frames == 5, "num_frames matches (5)");
            ok &= RUN_CHECK(runner, name, audioData2.samples.size() == 5, "samples size matches");
            return ok;
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            return false;
        }
    });

    runner.add("AudioIndex: audioData -> index -> audioData roundtrip (32-bit)", [&runner]() -> bool {
        const std::string    name      = "AudioIndex: audioData -> index -> audioData roundtrip (32-bit)";
        std::vector<int32_t> samples   = {0, 2147483647, -2147483647, 1000000, -1000000};
        auto                 audioData = AudioIndex::extractAudioDataFromSamples(samples, 48000, 32);
        try {
            auto idx        = AudioIndex::audioDataToIndex(audioData);
            auto audioData2 = AudioIndex::indexToAudioData(idx);
            bool ok         = true;
            ok &= RUN_CHECK(runner, name, audioData2.sample_rate == 48000, "sample_rate matches (48000)");
            ok &= RUN_CHECK(runner, name, audioData2.bit_rate == 32, "bit_rate matches (32-bit)");
            ok &= RUN_CHECK(runner, name, audioData2.num_channels == 1, "num_channels is 1");
            ok &= RUN_CHECK(runner, name, audioData2.num_frames == 5, "num_frames matches (5)");
            ok &= RUN_CHECK(runner, name, audioData2.samples.size() == 20, "samples size matches (5*4 bytes)");
            return ok;
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            return false;
        }
    });

    runner.add("AudioIndex: audioData -> index -> audioData roundtrip", [&runner]() -> bool {
        const std::string    name       = "AudioIndex: audioData -> index -> audioData roundtrip";
        std::vector<int32_t> samples    = {0, 12345, -12345, 30000, -30000};
        auto                 audioData  = AudioIndex::extractAudioDataFromSamples(samples, 44100, 16);
        auto                 idx        = AudioIndex::audioDataToIndex(audioData);
        auto                 audioData2 = AudioIndex::indexToAudioData(idx);

        bool ok = true;
        ok &= RUN_CHECK(runner, name, audioData2.sample_rate == 44100, "sample_rate is default 44100");
        ok &= RUN_CHECK(runner, name, audioData2.bit_rate == 16, "bit_rate is default 16");
        ok &= RUN_CHECK(runner, name, audioData2.num_channels == 1, "num_channels is default 1");
        ok &= RUN_CHECK(runner,
                        name,
                        audioData2.num_frames >= audioData.num_frames - 1 && audioData2.num_frames <= audioData.num_frames + 1,
                        "num_frames close match");
        ok &= RUN_CHECK(runner, name, audioData2.samples.size() > 0, "samples reconstructed");
        return ok;
    });

    runner.add("AudioIndex: exportAudioDataToWav and read back", [&runner]() -> bool {
        const std::string    name      = "AudioIndex: exportAudioDataToWav and read back";
        std::vector<int32_t> samples   = {0, 1000, -1000, 2000, -2000};
        auto                 audioData = AudioIndex::extractAudioDataFromSamples(samples, 22050, 16);
        TempFile             tmp(make_temp_path("temp_test.wav"));
        bool                 ok = true;
        try {
            AudioIndex::exportAudioDataToWav(audioData, tmp.path());
            auto audioData2 = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            ok &= RUN_CHECK(runner, name, audioData2.sample_rate == audioData.sample_rate, "sample_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.bit_rate == audioData.bit_rate, "bit_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.num_channels == audioData.num_channels, "num_channels match");
            ok &= RUN_CHECK(runner, name, audioData2.num_frames == audioData.num_frames, "num_frames match");
            ok &= RUN_CHECK(runner, name, audioData2.samples == audioData.samples, "samples match");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("AudioIndex: unsupported bit depth throws", [&runner]() -> bool {
        const std::string    name      = "AudioIndex: unsupported bit depth throws";
        std::vector<int32_t> samples   = {0, 1, 2};
        auto                 audioData = AudioIndex::extractAudioDataFromSamples(samples, 8000, 12); // 12-bit unsupported
        bool                 threw     = false;
        try {
            auto idx = AudioIndex::audioDataToIndex(audioData);
        } catch (const std::exception& e) {
            threw = true;
        }
        return RUN_CHECK(runner, name, threw, "audioDataToIndex should throw for unsupported bit depth");
    });

    runner.add("AudioIndex: empty samples roundtrip", [&runner]() -> bool {
        const std::string    name = "AudioIndex: empty samples roundtrip";
        std::vector<int32_t> samples; // empty
        auto                 audioData = AudioIndex::extractAudioDataFromSamples(samples, 48000, 16);
        bool                 ok        = true;
        ok &= RUN_CHECK(runner, name, audioData.num_frames == 0, "num_frames==0");
        try {
            auto idx        = AudioIndex::audioDataToIndex(audioData);
            auto audioData2 = AudioIndex::indexToAudioData(idx);
            ok &= RUN_CHECK(runner, name, audioData2.num_frames == 0, "roundtrip num_frames==0");
            ok &= RUN_CHECK(runner, name, audioData2.samples.empty(), "roundtrip samples empty");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("AudioIndex: silence duration preservation bug", [&runner]() -> bool {
        const std::string name = "AudioIndex: silence duration preservation bug";
        bool              ok   = true;
        try {
            std::vector<int32_t> silence_1sec(44100, 0);
            auto                 ad1  = AudioIndex::extractAudioDataFromSamples(silence_1sec, 44100, 16);
            auto                 idx1 = AudioIndex::audioDataToIndex(ad1);

            std::vector<int32_t> silence_2sec(88200, 0);
            auto                 ad2  = AudioIndex::extractAudioDataFromSamples(silence_2sec, 44100, 16);
            auto                 idx2 = AudioIndex::audioDataToIndex(ad2);

            bool indexes_equal = (idx1 == idx2);

            auto reconstructed1 = AudioIndex::indexToAudioData(idx1);
            auto reconstructed2 = AudioIndex::indexToAudioData(idx2);

            ok &= RUN_CHECK(runner, name, !indexes_equal, "FIX VERIFIED: 1sec and 2sec silence produce DIFFERENT indexes");
            ok &= RUN_CHECK(runner, name, reconstructed1.num_frames == 44100, "FIX: reconstructed1 num_frames is 44100 (1 second)");
            ok &= RUN_CHECK(runner, name, reconstructed2.num_frames == 88200, "FIX: reconstructed2 num_frames is 88200 (2 seconds)");
            ok &= RUN_CHECK(runner, name, reconstructed1.samples.size() == 88200, "FIX: reconstructed1 samples correct (44100*2 bytes)");
            ok &= RUN_CHECK(runner, name, reconstructed2.samples.size() == 176400, "FIX: reconstructed2 samples correct (88200*2 bytes)");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("AudioIndex: zero sampleRate duration is zero", [&runner]() -> bool {
        const std::string    name = "AudioIndex: zero sampleRate duration is zero";
        std::vector<int32_t> samples(10, 1000);
        auto                 ai = AudioIndex::fromAudioSamples(samples, 0, 16);
        bool                 ok = RUN_CHECK(runner, name, TestRunner::approxEqual(ai.getDuration(), 0.0, 1e-12), "duration==0 when sampleRate==0");
        return ok;
    });

    runner.add("AudioIndex: operator== equal objects", [&runner]() -> bool {
        const std::string    name    = "AudioIndex: operator== equal objects";
        std::vector<int32_t> samples = {100, -100, 200, -200};
        auto                 a       = AudioIndex::fromAudioSamples(samples, 44100, 16);
        auto                 b       = AudioIndex::fromAudioSamples(samples, 44100, 16);
        bool                 ok      = true;
        ok &= RUN_CHECK(runner, name, a == b, "a == b");
        ok &= RUN_CHECK(runner, name, !(a != b), "!(a != b)");
        return ok;
    });

    runner.add("AudioIndex: operator== different samples unequal", [&runner]() -> bool {
        const std::string    name = "AudioIndex: operator== different samples unequal";
        std::vector<int32_t> s1   = {0, 1, 2, 3};
        std::vector<int32_t> s2   = {0, 1, 2, 4};
        auto                 a    = AudioIndex::fromAudioSamples(s1, 44100, 16);
        auto                 b    = AudioIndex::fromAudioSamples(s2, 44100, 16);
        bool                 ok   = true;
        ok &= RUN_CHECK(runner, name, a != b, "a != b for different samples");
        ok &= RUN_CHECK(runner, name, !(a == b), "!(a == b)");
        return ok;
    });

    runner.add("AudioIndex: operator== different sampleRate unequal", [&runner]() -> bool {
        const std::string    name    = "AudioIndex: operator== different sampleRate unequal";
        std::vector<int32_t> samples = {10, 20, 30, 40};
        auto                 a       = AudioIndex::fromAudioSamples(samples, 44100, 16);
        auto                 b       = AudioIndex::fromAudioSamples(samples, 22050, 16);
        bool                 ok      = true;
        ok &= RUN_CHECK(runner, name, a != b, "a != b for different sample rates");
        ok &= RUN_CHECK(runner, name, !(a == b), "!(a == b)");
        return ok;
    });

    runner.add("AudioIndex: 16-bit edge values roundtrip", [&runner]() -> bool {
        const std::string     name = "AudioIndex: 16-bit edge values roundtrip";
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

        bool ok = true;
        try {
            auto idx        = AudioIndex::audioDataToIndex(audioData);
            auto audioData2 = AudioIndex::indexToAudioData(idx);
            ok &= RUN_CHECK(runner, name, audioData2.bit_rate == audioData.bit_rate, "bit_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.num_frames == audioData.num_frames, "num_frames match");
            ok &= RUN_CHECK(runner, name, audioData2.samples == audioData.samples, "samples content match");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("AudioIndex: 32-bit edge values roundtrip", [&runner]() -> bool {
        const std::string     name = "AudioIndex: 32-bit edge values roundtrip";
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

        bool ok = true;
        try {
            auto idx        = AudioIndex::audioDataToIndex(audioData);
            auto audioData2 = AudioIndex::indexToAudioData(idx);
            ok &= RUN_CHECK(runner, name, audioData2.bit_rate == 32, "bit_rate matches (32-bit)");
            ok &= RUN_CHECK(runner, name, audioData2.sample_rate == 48000, "sample_rate matches (48000)");
            ok &= RUN_CHECK(runner, name, audioData2.num_frames == 2, "num_frames matches (2)");
            ok &= RUN_CHECK(runner, name, audioData2.samples.size() == 8, "samples size matches (2*4 bytes)");
            ok &= RUN_CHECK(runner, name, audioData2.samples == audioData.samples, "samples content matches");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("AudioIndex: extractAudioDataFromSamples byte-order", [&runner]() -> bool {
        const std::string name = "AudioIndex: extractAudioDataFromSamples byte-order";
        bool              ok   = true;
        try {
            std::vector<int32_t> s16  = {0x1234};
            auto                 ad16 = AudioIndex::extractAudioDataFromSamples(s16, 44100, 16);
            ok &= RUN_CHECK(runner, name, ad16.samples.size() == 2, "16-bit sample produced 2 bytes");
            ok &= RUN_CHECK(runner, name, ad16.samples[0] == static_cast<uint8_t>(0x34), "16-bit LSB first byte");
            ok &= RUN_CHECK(runner, name, ad16.samples[1] == static_cast<uint8_t>(0x12), "16-bit MSB second byte");

            std::vector<int32_t> s32  = {0x0A0B0C0D};
            auto                 ad32 = AudioIndex::extractAudioDataFromSamples(s32, 48000, 32);
            ok &= RUN_CHECK(runner, name, ad32.samples.size() == 4, "32-bit sample produced 4 bytes");
            ok &= RUN_CHECK(runner, name, ad32.samples[0] == static_cast<uint8_t>(0x0D), "32-bit LSB first byte");
            ok &= RUN_CHECK(runner, name, ad32.samples[1] == static_cast<uint8_t>(0x0C), "32-bit byte 1");
            ok &= RUN_CHECK(runner, name, ad32.samples[2] == static_cast<uint8_t>(0x0B), "32-bit byte 2");
            ok &= RUN_CHECK(runner, name, ad32.samples[3] == static_cast<uint8_t>(0x0A), "32-bit MSB last byte");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("AudioIndex: serialization textual roundtrip", [&runner]() -> bool {
        const std::string name = "AudioIndex: serialization textual roundtrip";
        using boost::multiprecision::cpp_int;
        AudioIndex::AudioData audioData = AudioIndex::extractAudioDataFromSamples(std::vector<int32_t>{0, 12345, -12345}, 44100, 16);
        bool                  ok        = true;
        try {
            cpp_int idx = AudioIndex::audioDataToIndex(audioData);
            auto    s   = idx.convert_to<std::string>();
            cpp_int idx2(s);
            auto    audioData2 = AudioIndex::indexToAudioData(idx2);
            ok &= RUN_CHECK(runner, name, audioData2.sample_rate == audioData.sample_rate, "sample_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.bit_rate == audioData.bit_rate, "bit_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.num_frames == audioData.num_frames, "num_frames match");
            ok &= RUN_CHECK(runner, name, audioData2.samples == audioData.samples, "samples content match");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("AudioIndex: export_bits padding behavior", [&runner]() -> bool {
        const std::string name = "AudioIndex: export_bits padding behavior";
        using AudioBabel::AudioIndex;
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

        bool ok = true;
        try {
            auto idx        = AudioIndex::audioDataToIndex(audioData);
            auto audioData2 = AudioIndex::indexToAudioData(idx);

            ok &= RUN_CHECK(runner, name, audioData2.bit_rate == 16, "bit_rate is 16 (default)");
            ok &= RUN_CHECK(runner, name, audioData2.sample_rate == 44100, "sample_rate is 44100 (default)");
            ok &= RUN_CHECK(runner, name, audioData2.samples.size() >= audioData.samples.size() - 2, "samples size approximately correct");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });
}
