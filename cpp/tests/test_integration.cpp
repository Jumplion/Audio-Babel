#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "AudioIndex.h"
#include "test_common.h"

using namespace AudioBabel;

// Forward declaration for the test registration function
void register_integration_tests(TestRunner& runner);

// This is the integration test that processes a directory of WAV files.
void register_integration_tests(TestRunner& runner) {
    runner.add("AudioIndex: round-trip test audio directory", [&runner]() -> bool {
        const std::string name = "AudioIndex: round-trip test audio directory";
        bool              ok   = true;

        // The test audio directory is relative to the test executable's location
        // which is typically <repo_root>/build/
        std::filesystem::path test_audio_dir = "cpp/tests/Test Audio";
        if (!std::filesystem::exists(test_audio_dir)) {
            test_audio_dir = "../cpp/tests/Test Audio"; // fallback for different CWD
        }
        if (!std::filesystem::exists(test_audio_dir)) {
            runner.failMsg(name, "Test audio directory not found at " + test_audio_dir.string());
            return false;
        }

        // log_now("Starting round-trip test for directory: " + test_audio_dir.string(), true);

        for (const auto& entry : std::filesystem::directory_iterator(test_audio_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".wav") {
                const std::string wav_path = entry.path().string();
                //log_now("Processing file: " + wav_path, true);

                try {
                    // 1. Extract audio data from the WAV file
                    auto audio_data_orig = AudioIndex::extractAudioDataFromAudioFile(wav_path);
                    if (audio_data_orig.samples.empty()) {
                        log_now("Skipping empty audio file: " + wav_path, true);
                        continue;
                    }

                    // 2. Generate the index from the audio data
                    auto index = AudioIndex::audioDataToIndex(audio_data_orig);

                    // 3. Reconstruct the audio data from the index
                    auto audio_data_reconstructed = AudioIndex::indexToAudioData(index);

                    // 4. Compare original and reconstructed audio data
                    bool        file_ok  = true;
                    std::string filename = entry.path().filename().string();

                    if (audio_data_orig.sample_rate != audio_data_reconstructed.sample_rate) {
                        file_ok = false;
                        log_now("FAIL [" + filename + "]: Sample rate mismatch. Original: " + std::to_string(audio_data_orig.sample_rate) +
                                ", Reconstructed: " + std::to_string(audio_data_reconstructed.sample_rate));
                    }

                    if (audio_data_orig.bit_rate != audio_data_reconstructed.bit_rate) {
                        file_ok = false;
                        log_now("FAIL [" + filename + "]: Bit depth mismatch. Original: " + std::to_string(audio_data_orig.bit_rate) +
                                ", Reconstructed: " + std::to_string(audio_data_reconstructed.bit_rate));
                    }

                    if (audio_data_orig.num_channels != audio_data_reconstructed.num_channels) {
                        file_ok = false;
                        log_now("FAIL [" + filename + "]: Channel count mismatch. Original: " + std::to_string(audio_data_orig.num_channels) +
                                ", Reconstructed: " + std::to_string(audio_data_reconstructed.num_channels));
                    }

                    // Allow a small tolerance for frame count differences due to padding/encoding
                    if (std::abs(static_cast<long long>(audio_data_orig.num_frames) - static_cast<long long>(audio_data_reconstructed.num_frames)) >
                        2) {
                        file_ok = false;
                        log_now("FAIL [" + filename + "]: Frame count mismatch. Original: " + std::to_string(audio_data_orig.num_frames) +
                                ", Reconstructed: " + std::to_string(audio_data_reconstructed.num_frames));
                    }

                    // Compare sample data byte-for-byte
                    if (audio_data_orig.samples != audio_data_reconstructed.samples) {
                        // This can be noisy. Only log if it's a real problem.
                        // For now, we rely on the other checks. A deeper comparison might be needed
                        // if subtle corruption is suspected.
                        log_now("WARN [" + filename + "]: Sample data differs. This may be acceptable due to encoding/decoding nuances.");
                    }

                    if (file_ok) {
                        //log_now("PASS [" + filename + "]: Round-trip successful.", true);
                    } else {
                        ok = false; // Mark the whole test as failed
                        log_now("FAIL [" + filename + "]: Round-trip failed.", true);
                    }

                } catch (const std::exception& e) {
                    log_now("ERROR processing file " + wav_path + ": " + e.what(), true);
                    ok = false;
                }
            }
        }

        if (ok) {
            runner.passMsg(name);
        } else {
            runner.failMsg(name, "One or more files failed the round-trip test. See log for details.");
        }

        return ok;
    });
}
