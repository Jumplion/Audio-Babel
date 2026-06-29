/**
 * @file test_performance.cpp
 * @brief Performance benchmarks for the Audio Babel library.
 *
 * These tests measure execution time and throughput for key operations.
 * Unlike unit tests which verify correctness, these are raw measurements;
 * regression detection itself (comparing against a baseline with tolerance
 * for CI noise) lives in tools/node/compare-benchmarks.mjs, which consumes
 * the machine-readable build/performance_results.json this file writes.
 * See cpp/perf/results-schema.md for the JSON schema.
 *
 * Run these benchmarks in Release mode for accurate results:
 *   cmake -DCMAKE_BUILD_TYPE=Release ..
 *   make performance_benchmarks
 *   ./performance_benchmarks
 *
 * Results are written to: build/performance_results.txt (human-readable)
 * and build/performance_results.json (machine-readable).
 */

#include <FileIO.h>
#include <Index.h>
#include <IndexScramble.h>
#include <Utilities.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <vector>

#include "test_common.h"

#ifndef AUDIOBABEL_GIT_COMMIT
#    define AUDIOBABEL_GIT_COMMIT "unknown"
#endif
#ifndef AUDIOBABEL_BUILD_TYPE
#    define AUDIOBABEL_BUILD_TYPE "Unknown"
#endif

namespace {

struct PlatformInfo {
    std::string os;
    std::string compiler;
    std::string buildType;
    int         archBits;
};

auto detectPlatform() -> PlatformInfo {
    PlatformInfo info;

#if defined(_WIN32)
    info.os = "Windows";
#elif defined(__APPLE__)
    info.os = "macOS";
#elif defined(__linux__)
    info.os = "Linux";
#else
    info.os = "Unknown";
#endif

#if defined(__clang__)
    info.compiler = std::string("Clang ") + __clang_version__;
#elif defined(__GNUC__)
    info.compiler = "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__) + "." + std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    info.compiler = "MSVC " + std::to_string(_MSC_VER);
#else
    info.compiler = "Unknown";
#endif

    info.buildType = AUDIOBABEL_BUILD_TYPE;
    info.archBits  = static_cast<int>(sizeof(void*) * 8);

    return info;
}

/// Minimal JSON string escaping: benchmark names/categories/units are all
/// string literals controlled within this file, not external input, so this
/// only needs to defend against an embedded '"' or backslash, not full
/// Unicode/control-character escaping.
auto jsonEscape(const std::string& s) -> std::string {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

} // namespace

using namespace AudioBabel;
using namespace std::chrono;

// ============================================================================
// Benchmark Infrastructure
// ============================================================================

struct BenchmarkResult {
    std::string name;
    double      medianMs;
    double      minMs;
    double      maxMs;
    double      stddevMs;
    int         iterations;
    double      throughput;
    std::string throughputUnit;
    std::string category;
};

class BenchmarkRunner {
   public:
    std::vector<BenchmarkResult> results;
    std::ofstream                outFile;

    BenchmarkRunner(const std::string& outputPath) {
        outFile.open(outputPath, std::ios::out | std::ios::trunc);
        if (!outFile) {
            std::cerr << "Warning: Could not open benchmark output file: " << outputPath << '\n';
        }
    }

    ~BenchmarkRunner() {
        if (outFile.is_open()) {
            outFile.close();
        }
    }

    /**
     * @brief Run a benchmark multiple times and record median/min/max times.
     * @param name Benchmark name
     * @param category Category (e.g., "Index Operations")
     * @param iterations Number of times to run the benchmark
     * @param fn Function to benchmark
     * @param throughputCalc Optional throughput calculation function
     */
    template <typename Func, typename ThroughputFunc>
    void runBenchmark(const std::string& name, const std::string& category, int iterations, Func fn, ThroughputFunc throughputCalc) {
        std::vector<double> times;
        times.reserve(iterations);

        std::cout << "Running: " << name << " (" << iterations << " iterations)...\n";

        // Warm-up run (not counted)
        fn();

        // Timed runs
        for (int i = 0; i < iterations; ++i) {
            auto start = high_resolution_clock::now();
            fn();
            auto end = high_resolution_clock::now();
            times.push_back(duration_cast<nanoseconds>(end - start).count() / 1e6); // Convert to ms
        }

        // Calculate statistics
        std::sort(times.begin(), times.end());
        double medianMs = times[times.size() / 2];
        double minMs    = times.front();
        double maxMs    = times.back();

        double meanMs = std::accumulate(times.begin(), times.end(), 0.0) / static_cast<double>(times.size());
        double sqSum  = 0.0;
        for (double t : times) {
            double d = t - meanMs;
            sqSum += d * d;
        }
        double stddevMs = std::sqrt(sqSum / static_cast<double>(times.size()));

        // Calculate throughput
        auto [throughput, unit] = throughputCalc(medianMs);

        BenchmarkResult result;
        result.name           = name;
        result.category       = category;
        result.medianMs       = medianMs;
        result.minMs          = minMs;
        result.maxMs          = maxMs;
        result.stddevMs       = stddevMs;
        result.iterations     = iterations;
        result.throughput     = throughput;
        result.throughputUnit = unit;

        results.push_back(result);

        std::cout << "  Median: " << std::fixed << std::setprecision(3) << medianMs << " ms";
        if (!unit.empty()) {
            std::cout << "  (" << std::fixed << std::setprecision(1) << throughput << " " << unit << ")";
        }
        std::cout << '\n';
    }

    /**
     * @brief Generate the full benchmark report.
     */
    void generateReport() {
        if (!outFile.is_open()) {
            std::cerr << "Cannot write report: output file not open\n";
            return;
        }

        // Header
        auto        now = system_clock::now();
        std::time_t tt  = system_clock::to_time_t(now);

        PlatformInfo platform = detectPlatform();

        outFile << "=================================================\n";
        outFile << "PERFORMANCE BENCHMARK RESULTS\n";
        outFile << "Run Date: " << std::put_time(std::localtime(&tt), "%F %T") << '\n';
        outFile << "Platform: " << platform.os << " (" << platform.compiler << ", " << platform.archBits << "-bit, " << platform.buildType << ")\n";
        outFile << "Git Commit: " << AUDIOBABEL_GIT_COMMIT << '\n';
        outFile << "=================================================\n\n";

        // Group results by category
        std::string currentCategory;
        for (const auto& result : results) {
            if (result.category != currentCategory) {
                currentCategory = result.category;
                outFile << "[" << currentCategory << "]\n";
                outFile << "--------------------------------------------------\n";
            }

            outFile << std::left << std::setw(40) << result.name << ": ";
            outFile << std::right << std::fixed << std::setprecision(3) << std::setw(10) << result.medianMs << " ms";

            if (!result.throughputUnit.empty()) {
                outFile << "  (" << std::fixed << std::setprecision(1) << std::setw(10) << result.throughput << " " << result.throughputUnit << ")";
            }

            outFile << '\n';
        }

        // Summary
        outFile << "\n=================================================\n";
        outFile << "SUMMARY\n";
        outFile << "--------------------------------------------------\n";
        outFile << "Total Benchmarks Run: " << results.size() << '\n';
        outFile << "Performance data logged for regression tracking.\n";
        outFile << "=================================================\n";

        outFile.flush();
        std::cout << "\nBenchmark results written to output file.\n";
    }

    /**
     * @brief Write the machine-readable counterpart of generateReport(), consumed
     *        by tools/node/compare-benchmarks.mjs. Schema: cpp/perf/results-schema.md.
     */
    void generateJsonReport(const std::string& jsonPath) {
        std::ofstream jsonFile(jsonPath, std::ios::out | std::ios::trunc);
        if (!jsonFile) {
            std::cerr << "Cannot write JSON report: could not open " << jsonPath << '\n';
            return;
        }

        PlatformInfo platform = detectPlatform();

        auto        now = system_clock::now();
        std::time_t tt  = system_clock::to_time_t(now);
        std::ostringstream tsStream;
        tsStream << std::put_time(std::gmtime(&tt), "%FT%TZ");

        jsonFile << "{\n";
        jsonFile << "  \"schemaVersion\": 1,\n";
        jsonFile << "  \"generatedAt\": \"" << tsStream.str() << "\",\n";
        jsonFile << "  \"gitCommit\": \"" << jsonEscape(AUDIOBABEL_GIT_COMMIT) << "\",\n";
        jsonFile << "  \"platform\": {\n";
        jsonFile << "    \"os\": \"" << jsonEscape(platform.os) << "\",\n";
        jsonFile << "    \"compiler\": \"" << jsonEscape(platform.compiler) << "\",\n";
        jsonFile << "    \"buildType\": \"" << jsonEscape(platform.buildType) << "\",\n";
        jsonFile << "    \"archBits\": " << platform.archBits << "\n";
        jsonFile << "  },\n";
        jsonFile << "  \"benchmarks\": [\n";

        for (size_t i = 0; i < results.size(); ++i) {
            const auto& r = results[i];
            jsonFile << "    {\n";
            jsonFile << "      \"name\": \"" << jsonEscape(r.name) << "\",\n";
            jsonFile << "      \"category\": \"" << jsonEscape(r.category) << "\",\n";
            jsonFile << "      \"medianMs\": " << std::fixed << std::setprecision(6) << r.medianMs << ",\n";
            jsonFile << "      \"minMs\": " << r.minMs << ",\n";
            jsonFile << "      \"maxMs\": " << r.maxMs << ",\n";
            jsonFile << "      \"stddevMs\": " << r.stddevMs << ",\n";
            jsonFile << "      \"iterations\": " << r.iterations << ",\n";
            jsonFile << "      \"throughput\": " << r.throughput << ",\n";
            jsonFile << "      \"throughputUnit\": \"" << jsonEscape(r.throughputUnit) << "\"\n";
            jsonFile << "    }" << (i + 1 < results.size() ? "," : "") << "\n";
        }

        jsonFile << "  ]\n";
        jsonFile << "}\n";

        jsonFile.flush();
        std::cout << "Benchmark JSON results written to: " << jsonPath << '\n';
    }
};

// ============================================================================
// Benchmark Tests
// ============================================================================

// Helper: Generate synthetic audio samples
std::vector<int32_t> generateSyntheticAudio(size_t numSamples, uint16_t bitsPerSample) {
    std::vector<int32_t> samples;
    samples.reserve(numSamples);

    // Generate a simple sine wave
    double  frequency    = 440.0; // A4 note
    double  sampleRate   = 44100.0;
    int32_t maxAmplitude = (1 << (bitsPerSample - 1)) - 1;

    for (size_t i = 0; i < numSamples; ++i) {
        double t     = static_cast<double>(i) / sampleRate;
        double value = std::sin(2.0 * M_PI * frequency * t);
        samples.push_back(static_cast<int32_t>(value * maxAmplitude));
    }

    return samples;
}

// Helper: Generate random binary data
std::vector<uint8_t> generateRandomBytes(size_t numBytes) {
    std::vector<uint8_t> data;
    data.reserve(numBytes);
    for (size_t i = 0; i < numBytes; ++i) {
        data.push_back(static_cast<uint8_t>(i % 256));
    }
    return data;
}

// Helper: Pack int32 samples into 16-bit little-endian PCM bytes, without
// building a full FileIO::AudioData (Index::encode only needs the bytes).
std::vector<uint8_t> packSamples16(const std::vector<int32_t>& samples) {
    std::vector<uint8_t> bytes(samples.size() * 2);
    for (size_t i = 0; i < samples.size(); ++i) {
        auto v           = static_cast<int16_t>(samples[i]);
        bytes[i * 2]     = static_cast<uint8_t>(v & 0xFF);
        bytes[i * 2 + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    }
    return bytes;
}

void runIndexBenchmarks(BenchmarkRunner& runner) {
    const std::string category = "Index Operations";

    // 1.1-1.3d: Index generation across representative audio durations
    // (1s through the 600s/10-minute maximum duration).
    struct DurationCase {
        int durationSeconds;
        int iterations;
    };
    std::vector<DurationCase> durationCases = {
        {1, 1000},
        {30, 100},
        {120, 10},
        {240, 5},
        {480, 2},
        {600, 1},
    };

    for (const auto& dc : durationCases) {
        const size_t numSamples = 44100 * static_cast<size_t>(dc.durationSeconds);
        auto         samples    = generateSyntheticAudio(numSamples, 16);

        runner.runBenchmark(
            "Index Generation (" + std::to_string(dc.durationSeconds) + "s audio)",
            category,
            dc.iterations,
            [&samples]() {
                auto bytes = packSamples16(samples);
                auto index = Index::encode(bytes);
            },
            [numSamples](double ms) -> std::pair<double, std::string> {
                double samplesPerSec = (numSamples / (ms / 1000.0));
                return {samplesPerSec, "samples/sec"};
            });
    }

    // 1.4: Index Serialization
    {
        auto samples = generateSyntheticAudio(44100 * 30, 16);
        auto bytes   = packSamples16(samples);
        auto index   = Index::encode(bytes);

        runner.runBenchmark(
            "Index Serialization",
            category,
            10,
            [&index]() {
                std::vector<uint8_t> bytes;
                boost::multiprecision::export_bits(index, std::back_inserter(bytes), 8);
            },
            [](double ms) -> std::pair<double, std::string> {
                // Approximate size
                double kbPerSec = (100.0 / (ms / 1000.0)); // ~100KB typical
                return {kbPerSec, "KB/sec"};
            });
    }
}

void runLibraryPositionBenchmarks(BenchmarkRunner& runner) {
    const std::string category = "LibraryPosition Operations";

    // 4.1: Position calculation (small index)
    {
        boost::multiprecision::cpp_int smallIndex = 5000;

        runner.runBenchmark(
            "Position Calculation (small)",
            category,
            10,
            [&smallIndex]() { auto pos = calculateLibraryPosition(smallIndex); },
            [](double ms) -> std::pair<double, std::string> {
                double opsPerSec = (1.0 / (ms / 1000.0));
                return {opsPerSec, "calcs/sec"};
            });
    }

    // 4.2: Position calculation (large index)
    {
        boost::multiprecision::cpp_int largeIndex = boost::multiprecision::cpp_int("1000000000000000"); // 10^15

        runner.runBenchmark(
            "Position Calculation (large)",
            category,
            10,
            [&largeIndex]() { auto pos = calculateLibraryPosition(largeIndex); },
            [](double ms) -> std::pair<double, std::string> {
                double opsPerSec = (1.0 / (ms / 1000.0));
                return {opsPerSec, "calcs/sec"};
            });
    }

    // 4.3: Index reconstruction
    {
        LibraryPosition pos;
        pos.room  = "abc123";
        pos.wall  = 2;
        pos.shelf = 4;
        pos.album = 10;
        pos.track = 7;

        runner.runBenchmark(
            "Index Reconstruction",
            category,
            10,
            [&pos]() { auto idx = reconstructIndexFromPosition(pos); },
            [](double ms) -> std::pair<double, std::string> {
                double opsPerSec = (1.0 / (ms / 1000.0));
                return {opsPerSec, "recons/sec"};
            });
    }

    // 4.4: Batch position calculations
    {
        runner.runBenchmark(
            "Batch Position Calc (10,000 indexes)",
            category,
            10,
            []() {
                for (int i = 0; i < 10000; ++i) {
                    boost::multiprecision::cpp_int idx = i;
                    auto                           pos = calculateLibraryPosition(idx);
                }
            },
            [](double ms) -> std::pair<double, std::string> {
                double opsPerSec = (10000.0 / (ms / 1000.0));
                return {opsPerSec, "positions/sec"};
            });
    }
}

void runIndexMetadataBenchmarks(BenchmarkRunner& runner) {
    const std::string category = "IndexMetadata Operations";

    // 5.1: Metadata extraction (small index)
    {
        auto        samples     = generateSyntheticAudio(44100, 16); // 1 second
        auto        index       = Index::encode(packSamples16(samples));
        std::string base64Index = Utilities::indexToB64(index);

        runner.runBenchmark(
            "Metadata Extraction (small)",
            category,
            10,
            [&base64Index]() { auto metadata = IndexMetadata::extractMetadataFromIndex(base64Index); },
            [](double ms) -> std::pair<double, std::string> {
                double opsPerSec = (1.0 / (ms / 1000.0));
                return {opsPerSec, "extractions/sec"};
            });
    }

    // 5.2: Metadata extraction (large index)
    {
        auto        samples     = generateSyntheticAudio(44100 * 120, 16); // 120 seconds (max)
        auto        index       = Index::encode(packSamples16(samples));
        std::string base64Index = Utilities::indexToB64(index);

        runner.runBenchmark(
            "Metadata Extraction (large)",
            category,
            2,
            [&base64Index]() { auto metadata = IndexMetadata::extractMetadataFromIndex(base64Index); },
            [&base64Index](double ms) -> std::pair<double, std::string> {
                double mbPerSec = ((base64Index.size() / (1024.0 * 1024.0)) / (ms / 1000.0));
                return {mbPerSec, "MB/sec"};
            });
    }

    // 5.3: SVG cover generation
    {
        auto        samples     = generateSyntheticAudio(44100, 16);
        auto        index       = Index::encode(packSamples16(samples));
        std::string base64Index = Utilities::indexToB64(index);

        runner.runBenchmark(
            "SVG Cover Generation",
            category,
            1000,
            [&base64Index]() {
                auto metadata = IndexMetadata::extractMetadataFromIndex(base64Index);
                // Cover is generated inside extractMetadataFromIndex
            },
            [](double ms) -> std::pair<double, std::string> {
                double opsPerSec = (1.0 / (ms / 1000.0));
                return {opsPerSec, "SVGs/sec"};
            });
    }
}

void runIntegrationBenchmarks(BenchmarkRunner& runner) {
    const std::string category = "End-to-End Integration";

    // 6.4: Index → Position → Reconstruct → Verify
    {
        boost::multiprecision::cpp_int testIndex = 123456;

        runner.runBenchmark(
            "Index Roundtrip Verification",
            category,
            10000,
            [&testIndex]() {
                auto pos           = calculateLibraryPosition(testIndex);
                auto reconstructed = reconstructIndexFromPosition(pos);
                // Verify (in real code)
                bool valid = (reconstructed == testIndex);
                (void) valid; // Suppress unused warning
            },
            [](double ms) -> std::pair<double, std::string> {
                double opsPerSec = (1.0 / (ms / 1000.0));
                return {opsPerSec, "roundtrips/sec"};
            });
    }
}

void runScrambleBenchmarks(BenchmarkRunner& runner) {
    const std::string category = "IndexScramble Operations";
    const uint64_t    seed      = 0xC0FFEE123456ULL;

    boost::multiprecision::cpp_int smallIndex = 5000;

    auto        largeSamples = generateSyntheticAudio(44100 * 30, 16); // 30 seconds
    auto        largeIndex   = Index::encode(packSamples16(largeSamples));

    runner.runBenchmark(
        "Scramble Round Trip (small)",
        category,
        100,
        [&smallIndex, seed]() {
            auto scrambled = IndexScramble::scramble(smallIndex, seed);
            auto restored  = IndexScramble::unscramble(scrambled, seed);
            (void) restored;
        },
        [](double ms) -> std::pair<double, std::string> {
            double opsPerSec = (1.0 / (ms / 1000.0));
            return {opsPerSec, "scrambles/sec"};
        });

    runner.runBenchmark(
        "Scramble Round Trip (large)",
        category,
        10,
        [&largeIndex, seed]() {
            auto scrambled = IndexScramble::scramble(largeIndex, seed);
            auto restored  = IndexScramble::unscramble(scrambled, seed);
            (void) restored;
        },
        [](double ms) -> std::pair<double, std::string> {
            double opsPerSec = (1.0 / (ms / 1000.0));
            return {opsPerSec, "scrambles/sec"};
        });

    runner.runBenchmark(
        "Scramble Only (large)",
        category,
        10,
        [&largeIndex, seed]() { auto scrambled = IndexScramble::scramble(largeIndex, seed); },
        [](double ms) -> std::pair<double, std::string> {
            double opsPerSec = (1.0 / (ms / 1000.0));
            return {opsPerSec, "scrambles/sec"};
        });

    auto scrambledLarge = IndexScramble::scramble(largeIndex, seed);
    runner.runBenchmark(
        "Unscramble Only (large)",
        category,
        10,
        [&scrambledLarge, seed]() { auto restored = IndexScramble::unscramble(scrambledLarge, seed); },
        [](double ms) -> std::pair<double, std::string> {
            double opsPerSec = (1.0 / (ms / 1000.0));
            return {opsPerSec, "scrambles/sec"};
        });
}

void runUtilitiesBenchmarks(BenchmarkRunner& runner) {
    const std::string category = "Utilities Operations";

    boost::multiprecision::cpp_int smallIndex = 5000;

    auto largeSamples = generateSyntheticAudio(44100 * 30, 16); // 30 seconds
    auto largeIndex   = Index::encode(packSamples16(largeSamples));

    runner.runBenchmark(
        "Base64 Round Trip (small)",
        category,
        1000,
        [&smallIndex]() {
            auto encoded = Utilities::indexToB64(smallIndex);
            auto decoded = Utilities::b64ToIndex(encoded);
            (void) decoded;
        },
        [](double ms) -> std::pair<double, std::string> {
            double opsPerSec = (1.0 / (ms / 1000.0));
            return {opsPerSec, "roundtrips/sec"};
        });

    runner.runBenchmark(
        "Base64 Round Trip (large)",
        category,
        10,
        [&largeIndex]() {
            auto encoded = Utilities::indexToB64(largeIndex);
            auto decoded = Utilities::b64ToIndex(encoded);
            (void) decoded;
        },
        [](double ms) -> std::pair<double, std::string> {
            double opsPerSec = (1.0 / (ms / 1000.0));
            return {opsPerSec, "roundtrips/sec"};
        });

    std::string largeBase64 = Utilities::indexToB64(largeIndex);
    runner.runBenchmark(
        "Base64 Encode Only (large)",
        category,
        10,
        [&largeIndex]() { auto encoded = Utilities::indexToB64(largeIndex); },
        [&largeBase64](double ms) -> std::pair<double, std::string> {
            double charsPerSec = (largeBase64.size() / (ms / 1000.0));
            return {charsPerSec, "chars/sec"};
        });

    runner.runBenchmark(
        "Base64 Decode Only (large)",
        category,
        10,
        [&largeBase64]() { auto decoded = Utilities::b64ToIndex(largeBase64); },
        [&largeBase64](double ms) -> std::pair<double, std::string> {
            double charsPerSec = (largeBase64.size() / (ms / 1000.0));
            return {charsPerSec, "chars/sec"};
        });
}

void runFileIoBenchmarks(BenchmarkRunner& runner) {
    const std::string category = "FileIO Operations";

    auto samples   = generateSyntheticAudio(44100 * 5, 16); // 5 seconds
    auto audioData = fromSamples(samples, 44100, 16);

    double payloadMb = audioData.samples.size() / (1024.0 * 1024.0);

    {
        TempFile writeTarget(make_temp_path("perf_write.wav"));

        runner.runBenchmark(
            "WAV Write (5s audio)",
            category,
            20,
            [&audioData, &writeTarget]() { FileIO::writeWav(audioData, writeTarget.path()); },
            [payloadMb](double ms) -> std::pair<double, std::string> {
                double mbPerSec = (payloadMb / (ms / 1000.0));
                return {mbPerSec, "MB/sec"};
            });
    }

    {
        TempFile readTarget(make_temp_path("perf_read.wav"));
        FileIO::writeWav(audioData, readTarget.path()); // untimed setup

        runner.runBenchmark(
            "WAV Read (5s audio)",
            category,
            20,
            [&readTarget]() { auto loaded = FileIO::readWav(readTarget.path()); },
            [payloadMb](double ms) -> std::pair<double, std::string> {
                double mbPerSec = (payloadMb / (ms / 1000.0));
                return {mbPerSec, "MB/sec"};
            });
    }
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main() {
    std::cout << "=================================================\n";
    std::cout << "AUDIO BABEL PERFORMANCE BENCHMARKS\n";
    std::cout << "=================================================\n";
    std::cout << "Note: Run in Release mode for accurate results.\n";
    std::cout << "Results will be written to: performance_results.{txt,json} "
                  "(run this binary from inside build/ so they land at build/performance_results.*)\n\n";

    BenchmarkRunner runner("performance_results.txt");

    // Run all benchmark categories
    std::cout << "\n[1/7] Running Index benchmarks...\n";
    runIndexBenchmarks(runner);

    std::cout << "\n[2/7] Running LibraryPosition benchmarks...\n";
    runLibraryPositionBenchmarks(runner);

    std::cout << "\n[3/7] Running IndexMetadata benchmarks...\n";
    runIndexMetadataBenchmarks(runner);

    std::cout << "\n[4/7] Running Integration benchmarks...\n";
    runIntegrationBenchmarks(runner);

    std::cout << "\n[5/7] Running IndexScramble benchmarks...\n";
    runScrambleBenchmarks(runner);

    std::cout << "\n[6/7] Running Utilities benchmarks...\n";
    runUtilitiesBenchmarks(runner);

    std::cout << "\n[7/7] Running FileIO benchmarks...\n";
    runFileIoBenchmarks(runner);

    // Generate reports
    std::cout << "\n=================================================\n";
    std::cout << "Generating report...\n";
    runner.generateReport();
    runner.generateJsonReport("performance_results.json");

    std::cout << "\nAll benchmarks completed successfully!\n";
    std::cout << "=================================================\n";

    return 0;
}
