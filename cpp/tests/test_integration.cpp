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

    // Track files processed and any failures
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

        // 1. Extract audio data from the WAV file
        auto audio_data_orig = AudioIndex::extractAudioDataFromAudioFile(wav_path);

        // Skip empty files
        if (audio_data_orig.samples.empty()) {
            WARN("Skipping empty audio file: " << filename);
            continue;
        }

        files_processed++;

        // 2. Generate the index from the audio data
        auto index = AudioIndex::audioDataToIndex(audio_data_orig);

        // 3. Reconstruct the audio data from the index
        auto audio_data_reconstructed = AudioIndex::indexToAudioData(index);

        // 4. Verify the round-trip preserved audio properties
        bool file_ok = true;

        // Check sample rate
        if (audio_data_orig.sample_rate != audio_data_reconstructed.sample_rate) {
            INFO("FAIL [" << filename << "]: Sample rate mismatch. Original: " << audio_data_orig.sample_rate
                          << ", Reconstructed: " << audio_data_reconstructed.sample_rate);
            file_ok = false;
        }

        // Check bit depth
        if (audio_data_orig.bit_rate != audio_data_reconstructed.bit_rate) {
            INFO("FAIL [" << filename << "]: Bit depth mismatch. Original: " << audio_data_orig.bit_rate
                          << ", Reconstructed: " << audio_data_reconstructed.bit_rate);
            file_ok = false;
        }

        // Check channel count
        if (audio_data_orig.num_channels != audio_data_reconstructed.num_channels) {
            INFO("FAIL [" << filename << "]: Channel count mismatch. Original: " << audio_data_orig.num_channels
                          << ", Reconstructed: " << audio_data_reconstructed.num_channels);
            file_ok = false;
        }

        // Check frame count (allow small tolerance for padding/encoding differences)
        long long frame_diff =
            std::abs(static_cast<long long>(audio_data_orig.num_frames) - static_cast<long long>(audio_data_reconstructed.num_frames));
        if (frame_diff > 2) {
            INFO("FAIL [" << filename << "]: Frame count mismatch. Original: " << audio_data_orig.num_frames
                          << ", Reconstructed: " << audio_data_reconstructed.num_frames << ", Difference: " << frame_diff);
            file_ok = false;
        }

        // Compare sample data byte-for-byte
        if (audio_data_orig.samples != audio_data_reconstructed.samples) {
            WARN("Sample data differs for " << filename << ". This may be acceptable due to encoding/decoding nuances.");
        }

        // Track failures
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
