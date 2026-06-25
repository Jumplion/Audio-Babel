/**
 * @file test_performance.cpp
 * @brief Performance benchmarks for the Audio Babel library.
 * 
 * These tests measure execution time and throughput for key operations.
 * Unlike unit tests which verify correctness, these tests track performance
 * regression over time.
 * 
 * Run these benchmarks in Release mode for accurate results:
 *   cmake -DCMAKE_BUILD_TYPE=Release ..
 *   make performance_benchmarks
 *   ./performance_benchmarks
 * 
 * Results are written to: build/performance_results.txt
 */

#include <Index.h>

#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <vector>

#include "test_common.h"

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

        // Calculate throughput
        auto [throughput, unit] = throughputCalc(medianMs);

        BenchmarkResult result;
        result.name           = name;
        result.category       = category;
        result.medianMs       = medianMs;
        result.minMs          = minMs;
        result.maxMs          = maxMs;
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

        outFile << "=================================================\n";
        outFile << "PERFORMANCE BENCHMARK RESULTS\n";
        outFile << "Run Date: " << std::put_time(std::localtime(&tt), "%F %T") << '\n';
        outFile << "Platform: Windows (MinGW GCC)\n";
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

// ============================================================================
// Main Entry Point
// ============================================================================

int main() {
    std::cout << "=================================================\n";
    std::cout << "AUDIO BABEL PERFORMANCE BENCHMARKS\n";
    std::cout << "=================================================\n";
    std::cout << "Note: Run in Release mode for accurate results.\n";
    std::cout << "Results will be written to: build/performance_results.txt\n\n";

    BenchmarkRunner runner("build/performance_results.txt");

    // Run all benchmark categories
    std::cout << "\n[1/4] Running Index benchmarks...\n";
    runIndexBenchmarks(runner);

    std::cout << "\n[2/4] Running LibraryPosition benchmarks...\n";
    runLibraryPositionBenchmarks(runner);

    std::cout << "\n[3/4] Running IndexMetadata benchmarks...\n";
    runIndexMetadataBenchmarks(runner);

    std::cout << "\n[4/4] Running Integration benchmarks...\n";
    runIntegrationBenchmarks(runner);

    // Generate report
    std::cout << "\n=================================================\n";
    std::cout << "Generating report...\n";
    runner.generateReport();

    std::cout << "\nAll benchmarks completed successfully!\n";
    std::cout << "=================================================\n";

    return 0;
}
