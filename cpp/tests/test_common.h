/**
 * @file test_common.h
 * @brief Shared test infrastructure for the Audio Babel test suite.
 *
 * This header provides small, framework-free helpers used alongside Catch2
 * across test modules. It intentionally avoids external dependencies so it
 * can be built with the project's normal toolchain.
 *
 * @section test_infrastructure Test Infrastructure
 * - make_temp_path: Temporary file path generation
 * - TempFile: RAII temporary file management
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "FileIO.h"
#include "Index.h"
#include "IndexMetadata.h"
#include "LibraryPosition.h"
#include "Utilities.h"

#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

using namespace AudioBabel;

/**
 * @brief Create a unique temporary filepath in the OS temp directory.
 * @param basename Base name for the temporary file
 * @return Absolute path to temporary file
 * 
 * @note The file path is unique per thread and timestamp to avoid collisions
 *       in parallel test runs. Falls back to current directory if temp dir
 *       is unavailable.
 */
inline auto make_temp_path(const std::string& basename) -> std::string {
    try {
        auto p = std::filesystem::temp_directory_path();
        // create a small, reasonably-unique suffix to avoid collisions in parallel runs
        auto               now      = std::chrono::steady_clock::now().time_since_epoch().count();
        auto               tid_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
        std::ostringstream ss;
        ss << basename << "_" << now << "_" << tid_hash;
        p /= ss.str();
        return p.string();
    } catch (...) {
        // fallback to current directory
        auto               now      = std::chrono::steady_clock::now().time_since_epoch().count();
        auto               tid_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
        std::ostringstream ss;
        ss << basename << "_" << now << "_" << tid_hash;
        return ss.str();
    }
}

/**
 * @struct TempFile
 * @brief RAII wrapper for temporary files that automatically deletes on destruction.
 * 
 * @par Usage Example
 * @code
 * {
 *     TempFile temp(make_temp_path("mytest.wav"));
 *     // Use temp.path() for file operations
 * } // File automatically deleted here
 * @endcode
 */
struct TempFile {
    std::filesystem::path p;

    explicit TempFile(const std::string& s) : p(s) {}

    ~TempFile() {
        try {
            if (!p.empty() && std::filesystem::exists(p)) {
                std::filesystem::remove(p);
            }
        } catch (...) {
            // Best-effort cleanup; don't throw from destructor
        }
    }

    [[nodiscard]] auto path() const -> std::string {
        return p.string();
    }
};

/**
 * @brief Build a little-endian PCM sample payload from a list of unsigned 16-bit samples.
 */
inline auto makePayload(const std::vector<uint16_t>& samples) -> std::vector<uint8_t> {
    std::vector<uint8_t> bytes;
    bytes.reserve(samples.size() * 2);
    for (uint16_t v : samples) {
        bytes.push_back(static_cast<uint8_t>(v & 0xFF));
        bytes.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }
    return bytes;
}

#endif // TEST_COMMON_H
