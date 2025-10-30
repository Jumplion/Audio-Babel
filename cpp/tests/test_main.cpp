/**
 * @file test_main.cpp
 * @brief Main entry point for the Audio Babel test suite.
 * 
 * This file orchestrates all test modules and runs them sequentially.
 * The actual test implementations are split into logical modules:
 * - test_base64.cpp: Base64 encoding/decoding tests
 * - test_metadata.cpp: IndexMetadata extraction tests
 * - test_library_position.cpp: LibraryPosition calculation tests
 * - test_wav_parsing.cpp: WAV file parsing tests
 * - test_audio_index.cpp: Core AudioIndex serialization tests
 * - test_integration.cpp: End-to-end integration tests
 */

#include "test_common.h"

// Global log file (defined in test_common.h)
std::ofstream g_log;

// Forward declarations for test registration functions
void register_base64_tests(TestRunner& runner);
void register_metadata_tests(TestRunner& runner);
void register_library_position_tests(TestRunner& runner);
void register_wav_parsing_tests(TestRunner& runner);
void register_audio_index_tests(TestRunner& runner);
void register_integration_tests(TestRunner& runner);

auto main(int argc, char** argv) -> int {
    TestRunner runner;

    // Initialize global log file
    g_log.open("test_log.txt", std::ios::app);
    if (!g_log) {
        std::cerr << "Warning: Could not open test_log.txt for logging\n";
    } else {
        log_now("========== Test run started ==========", true);
    }

    // Register all test modules
    register_base64_tests(runner);
    register_metadata_tests(runner);
    register_library_position_tests(runner);
    register_wav_parsing_tests(runner);
    register_audio_index_tests(runner);
    register_integration_tests(runner);

    // Run tests with optional filter
    std::string filter;
    if (argc > 1) {
        filter = argv[1];
        std::cout << "Running tests matching filter: \"" << filter << "\"\n\n";
    }

    runner.runAll(filter);

    // Clean up
    if (g_log) {
        log_now("========== Test run completed ==========", true);
        log_now("Summary: " + std::to_string(runner.passed) + " passed, " + std::to_string(runner.failed) + " failed");
        g_log.close();
    }

    return (runner.failed == 0) ? 0 : 1;
}
