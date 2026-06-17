/**
 * @file test_integration_new.cpp
 * @brief Integration tests for AudioIndex round-trip processing (Catch2 version).
 *
 * Tests the full pipeline of extracting audio data from WAV files,
 * converting to index, and reconstructing the audio data.
 */

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "AudioIndex.h"
#include "test_common.h"

using namespace AudioBabel;
using boost::multiprecision::cpp_int;
using namespace LibraryConstants;

namespace {

/// Find the test audio directory, trying multiple possible locations
std::filesystem::path find_test_audio_dir() {
    // Try relative to build directory first
    std::filesystem::path test_audio_dir = "cpp/tests/Test Audio";
    if (std::filesystem::exists(test_audio_dir)) {
        return test_audio_dir;
    }

    // Try fallback location
    test_audio_dir = "../cpp/tests/Test Audio";
    if (std::filesystem::exists(test_audio_dir)) {
        return test_audio_dir;
    }

    // Return empty path if not found
    return {};
}

} // anonymous namespace

TEST_CASE("AudioIndex: round-trip test audio directory", "[integration][roundtrip][directory]") {
    // Find the test audio directory
    auto test_audio_dir = find_test_audio_dir();
    REQUIRE_FALSE(test_audio_dir.empty());
    REQUIRE(std::filesystem::exists(test_audio_dir));

    INFO("Testing audio files in directory: " << test_audio_dir.string());

    // The payload-only bijection uses per-sample cpp_int arithmetic, which is
    // intentionally O(L^2) (see the TODO in AudioIndex.cpp). That is fine for
    // short clips but would take many minutes on the multi-MB sample library, so
    // for each real file we round-trip only a bounded prefix of its PCM payload.
    // This still exercises real WAV-sourced sample bytes through the full
    // extract -> index -> reconstruct pipeline. The default decode header
    // (PCM, 44100 Hz, 16-bit, mono) is verified on the reconstruction.
    constexpr size_t kMaxRoundTripBytes = 8000; // 4000 samples at 16-bit

    int files_processed = 0;
    int files_failed    = 0;

    // Process each WAV file in the directory
    for (const auto& entry : std::filesystem::directory_iterator(test_audio_dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".wav") {
            continue;
        }

        const std::string wav_path = entry.path().string();
        const std::string filename = entry.path().filename().string();

        INFO("Processing file: " << filename);

        // 1. Extract audio data from the WAV file (full file; parsing is fast).
        auto audio_data_orig = AudioIndex::extractAudioDataFromAudioFile(wav_path);

        // Skip empty files
        if (audio_data_orig.samples.empty()) {
            WARN("Skipping empty audio file: " << filename);
            continue;
        }

        files_processed++;

        // 2. Build a bounded-prefix payload (whole 16-bit samples) to round-trip.
        AudioIndex::AudioData prefix{};
        prefix.audio_format = audio_data_orig.audio_format;
        prefix.sample_rate  = audio_data_orig.sample_rate;
        prefix.bit_rate     = audio_data_orig.bit_rate;
        prefix.num_channels = audio_data_orig.num_channels;
        size_t take         = std::min(audio_data_orig.samples.size(), kMaxRoundTripBytes);
        take -= (take % 2); // keep whole 16-bit samples
        prefix.samples.assign(audio_data_orig.samples.begin(), audio_data_orig.samples.begin() + take);
        prefix.num_frames = take / 2;

        // 3. index round-trip of the prefix payload.
        auto index                    = AudioIndex::audioDataToIndex(prefix);
        auto audio_data_reconstructed = AudioIndex::indexToAudioData(index);

        bool file_ok = true;

        // The reconstruction always carries the fixed default header.
        if (audio_data_reconstructed.sample_rate != 44100 || audio_data_reconstructed.bit_rate != 16 ||
            audio_data_reconstructed.num_channels != 1 || audio_data_reconstructed.audio_format != 1) {
            INFO("FAIL [" << filename << "]: default header not applied on reconstruction");
            file_ok = false;
        }

        // The prefix sample bytes must be reproduced exactly.
        if (audio_data_reconstructed.samples != prefix.samples) {
            INFO("FAIL [" << filename << "]: prefix sample bytes not reproduced exactly");
            file_ok = false;
        }

        // The decoded sample count must match the prefix exactly.
        if (audio_data_reconstructed.num_frames != prefix.num_frames) {
            INFO("FAIL [" << filename << "]: frame count mismatch. Expected: " << prefix.num_frames
                          << ", Reconstructed: " << audio_data_reconstructed.num_frames);
            file_ok = false;
        }

        if (!file_ok) {
            files_failed++;
        }
    }

    // Ensure we processed at least some files
    REQUIRE(files_processed > 0);
    INFO("Processed " << files_processed << " files");

    // Report if any files failed
    if (files_failed > 0) {
        FAIL("Round-trip failed for " << files_failed << " out of " << files_processed << " files");
    }
}
