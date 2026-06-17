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

    // The payload-only bijection is O(N) (see AudioIndex.cpp), so full multi-MB
    // files round-trip in milliseconds. For each file we extract the PCM payload,
    // index it, reconstruct it, and require the sample bytes to be reproduced
    // exactly. The reconstruction always carries the fixed default header
    // (PCM, 44100 Hz, 16-bit, mono); the original sample format is intentionally
    // not preserved by the payload-only index.
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

        // 1. Extract audio data from the WAV file.
        auto audio_data_orig = AudioIndex::extractAudioDataFromAudioFile(wav_path);

        // Skip empty files
        if (audio_data_orig.samples.empty()) {
            WARN("Skipping empty audio file: " << filename);
            continue;
        }

        // Whole 16-bit samples are required for an exact payload bijection.
        if (audio_data_orig.samples.size() % 2 != 0) {
            WARN("Skipping file with odd payload byte count: " << filename);
            continue;
        }

        files_processed++;

        // 2. Full round-trip through the index.
        auto index                    = AudioIndex::audioDataToIndex(audio_data_orig);
        auto audio_data_reconstructed = AudioIndex::indexToAudioData(index);

        bool file_ok = true;

        // The reconstruction always carries the fixed default header.
        if (audio_data_reconstructed.sample_rate != 44100 || audio_data_reconstructed.bit_rate != 16 ||
            audio_data_reconstructed.num_channels != 1 || audio_data_reconstructed.audio_format != 1) {
            INFO("FAIL [" << filename << "]: default header not applied on reconstruction");
            file_ok = false;
        }

        // The PCM sample bytes must be reproduced exactly.
        if (audio_data_reconstructed.samples != audio_data_orig.samples) {
            INFO("FAIL [" << filename << "]: sample bytes not reproduced exactly");
            file_ok = false;
        }

        // The decoded sample count must match exactly.
        if (audio_data_reconstructed.num_frames != audio_data_orig.samples.size() / 2) {
            INFO("FAIL [" << filename << "]: frame count mismatch. Expected: " << (audio_data_orig.samples.size() / 2)
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
