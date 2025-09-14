#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>
#include <vector>

#include "AudioIndex.h"
#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

using namespace AudioBabel;

// Global log file used by the test runner and integration tests
static std::ofstream g_log;

static void log_now(const std::string& msg, bool printToConsole = false) {
    if (!g_log) {
        return;
    }
    auto        now = std::chrono::system_clock::now();
    std::time_t tt  = std::chrono::system_clock::to_time_t(now);
    g_log << "[" << std::put_time(std::localtime(&tt), "%F %T") << "] " << msg << '\n';
    g_log.flush();

    if (printToConsole) {
        std::cout << "[" << std::put_time(std::localtime(&tt), "%F %T") << "] " << msg << '\n';
    }
}

// Helper to create a temporary filepath in the OS temp directory.
static auto make_temp_path(const std::string& basename) -> std::string {
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

// RAII temporary file: removes the file on destruction (best-effort)
struct TempFile {
    std::filesystem::path p;
    explicit TempFile(const std::string& s) : p(s) {}
    ~TempFile() {
        try {
            if (!p.empty() && std::filesystem::exists(p)) {
                std::filesystem::remove(p);
            }
        } catch (...) {
        }
    }
    [[nodiscard]] auto path() const -> std::string {
        return p.string();
    }
};

// ---------------------------------------------------------------------------
// Test harness for Speaker-of-Babel (cpp/tests/test_main.cpp)
//
// This file implements a minimal, framework-free test runner used by the
// repository CI and local development. It intentionally avoids external
// dependencies so it can be built with the project's normal toolchain.
//
// Sections:
//  - TestRunner: small harness that tracks pass/fail counts
//  - CHECK helper: assertion helper used inside tests
//  - testAudioIndex_impl: unit tests for AudioIndex basic behavior
//  - WAV loader: tiny helper to load WAV files for integration tests
//  - testAudioIndex_wav_impl: enumerates WAV files and logs results
//  - main: registers tests and runs them
// ---------------------------------------------------------------------------

// Lightweight test harness (no external framework)
struct TestRunner {
    int                                          passed = 0;
    int                                          failed = 0;
    std::map<std::string, std::function<bool()>> tests;

    void add(const std::string& name, const std::function<bool()>& fn) {
        tests[name] = fn;
    }

    /**
     * Add a test to the runner.
     * @param name Human-readable test name shown in console output.
     * @param fn   Callable returning true on success, false on failure.
     */

    static auto approxEqual(double a, double b, double tol = 1e-6) -> bool {
        return std::fabs(a - b) <= tol;
    }

    static auto vecNotEmpty(const std::vector<int32_t>& v) -> bool {
        return !v.empty();
    }

    void failMsg(const std::string& test, const std::string& msg) {
        std::cout << "  ✗ " << test << " — " << msg << '\n';
        ++failed;
    }

    /**
     * Called by tests to record an individual failure message. Increments
     * the failure counter and prints a short failure line.
     */

    void passMsg(const std::string& test) {
        std::cout << "  ✓ " << test << '\n';
        ++passed;
    }

    /**
     * Called by the harness when a test completes successfully. Prints
     * a short success line and increments the pass counter.
     */

    auto runOne(const std::string& name) -> bool {
        auto it = tests.find(name);
        if (it == tests.end()) {
            std::cout << "Test not found: " << name << '\n';
            return false;
        }
        // No spinner: run the test and measure duration. Print one line with
        // the elapsed time and the final result. To avoid double-counting
        // assertion failures (which call runner.failMsg), capture the
        // failure count before/after running the test.
        auto        failed_before = static_cast<size_t>(failed);
        bool        ok            = false;
        std::string exceptionMsg;
        auto        t0 = std::chrono::steady_clock::now();
        log_now(std::string("START TEST: ") + name);
        try {
            ok = it->second();
        } catch (const std::exception& e) {
            ok           = false;
            exceptionMsg = std::string("exception: ") + e.what();
        } catch (...) {
            ok           = false;
            exceptionMsg = "unknown exception";
        }
        auto t1 = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        // If the test passed, increment pass counter once. If it failed but
        // already produced failure messages (runner.failed > failed_before),
        // don't increment failed again; otherwise increment failed once.
        if (ok) {
            ++passed;
            log_now(std::string("PASS: ") + name + " (" + std::to_string(ms) + "ms)");
        } else {
            log_now(std::string("FAIL: ") + name + " (" + std::to_string(ms) + "ms)");
            if (failed > static_cast<int>(failed_before)) {
                // failure already reported by assertions; just print summary
            } else {
                ++failed;
            }
        }
        return ok;
    }

    void runAll(const std::string& filter = "") {
        for (auto& kv : tests) {
            if (!filter.empty() && kv.first.find(filter) == std::string::npos) {
                continue;
            }
            // Log the start of the test
            log_now(std::string("============== RUNNING TEST: [") + kv.first + "] ==============");
            std::cout << "============== RUNNING TEST: [" << kv.first << "] ==============" << '\n';
            runOne(kv.first);
        }
        std::cout << "\nSummary: " << passed << " passed, " << failed << " failed" << '\n';
    }
};

// Helpers for assertions used inside tests
static auto CHECK(bool cond, TestRunner& runner, const std::string& test, const std::string& msg = "") -> bool {
    if (!cond) {
        runner.failMsg(test, msg.empty() ? "check failed" : msg);
        return false;
    }
    return true;
}

// Shared helper: run CHECK and print per-assertion status for a named test
static auto RUN_CHECK(TestRunner& runner, const std::string& testName, bool cond, const std::string& msg) -> bool {
    bool ok = CHECK(cond, runner, testName, msg);
    if (ok) {
        std::cout << "  [OK]   " << msg << '\n';
    } else {
        std::cout << "  [FAIL] " << msg << '\n';
    }
    return ok;
}

auto main(int argc, char** argv) -> int {
    TestRunner runner;
    // Register split AudioIndex unit tests

    // -- Unit tests for AudioIndex basic behavior
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

    runner.add("AudioIndex: audioData -> index -> audioData roundtrip", [&runner]() -> bool {
        const std::string    name       = "AudioIndex: audioData -> index -> audioData roundtrip";
        std::vector<int32_t> samples    = {0, 12345, -12345, 30000, -30000};
        auto                 audioData  = AudioIndex::extractAudioDataFromSamples(samples, 44100, 16);
        auto                 idx        = AudioIndex::audioDataToIndex(audioData);
        auto                 audioData2 = AudioIndex::indexToAudioData(idx);

        bool ok = true;
        ok &= RUN_CHECK(runner, name, audioData2.sample_rate == audioData.sample_rate, "sample_rate match");
        ok &= RUN_CHECK(runner, name, audioData2.bit_rate == audioData.bit_rate, "bit_rate match");
        ok &= RUN_CHECK(runner, name, audioData2.num_channels == audioData.num_channels, "num_channels match");
        ok &= RUN_CHECK(runner, name, audioData2.num_frames == audioData.num_frames, "num_frames match");
        ok &= RUN_CHECK(runner, name, audioData2.samples.size() == audioData.samples.size(), "samples size match");
        ok &= RUN_CHECK(runner, name, audioData2.samples == audioData.samples, "samples content match");
        return ok;
    });

    runner.add("AudioIndex: fromAudioSamples and getters", [&runner]() -> bool {
        const std::string    name = "AudioIndex: fromAudioSamples and getters";
        std::vector<int32_t> samples(44100); // 1 second of silence at 44.1k
        for (int& sample : samples) {
            sample = 0;
        }
        auto ai = AudioIndex::fromAudioSamples(samples, 44100, 16);

        bool ok = true;
        ok &= RUN_CHECK(runner, name, ai.getSampleRate() == 44100, "getSampleRate");
        ok &= RUN_CHECK(runner, name, ai.getBitDepth() == 16, "getBitDepth");
        ok &= RUN_CHECK(runner, name, TestRunner::approxEqual(ai.getDuration(), 1.0, 1e-6), "getDuration ~ 1s");
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
        // file cleanup handled by TempFile destructor
        return ok;
    });

    // ------------------ Negative / edge-case tests ------------------
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

    runner.add("AudioIndex: zero sampleRate duration is zero", [&runner]() -> bool {
        const std::string    name = "AudioIndex: zero sampleRate duration is zero";
        std::vector<int32_t> samples(10, 1000);
        auto                 ai = AudioIndex::fromAudioSamples(samples, 0, 16);
        bool                 ok = RUN_CHECK(runner, name, TestRunner::approxEqual(ai.getDuration(), 0.0, 1e-12), "duration==0 when sampleRate==0");
        return ok;
    });

    runner.add("AudioIndex: malformed header bit depth rejected", [&runner]() -> bool {
        const std::string name = "AudioIndex: malformed header bit depth rejected";
        using boost::multiprecision::cpp_int;

        // Build a header with unsupported bit depth (7)
        std::vector<uint8_t> header_buf;
        uint32_t             sr = 44100;
        header_buf.push_back(static_cast<uint8_t>((sr >> 24) & 0xFF));
        header_buf.push_back(static_cast<uint8_t>((sr >> 16) & 0xFF));
        header_buf.push_back(static_cast<uint8_t>((sr >> 8) & 0xFF));
        header_buf.push_back(static_cast<uint8_t>((sr >> 0) & 0xFF));
        uint16_t bd = 7;
        header_buf.push_back(static_cast<uint8_t>((bd >> 8) & 0xFF));
        header_buf.push_back(static_cast<uint8_t>((bd >> 0) & 0xFF));
        uint16_t nc = 1;
        header_buf.push_back(static_cast<uint8_t>((nc >> 8) & 0xFF));
        header_buf.push_back(static_cast<uint8_t>((nc >> 0) & 0xFF));
        uint64_t nf = 1;
        for (int i = 7; i >= 0; --i) {
            header_buf.push_back(static_cast<uint8_t>((nf >> (i * 8)) & 0xFF));
        }

        cpp_int header_int = 0;
        for (uint8_t b : header_buf) {
            header_int <<= 8;
            header_int |= cpp_int(static_cast<uint32_t>(b));
        }
        // pcm_int = 0
        cpp_int idx = header_int;

        bool threw = false;
        try {
            auto audioData = AudioIndex::indexToAudioData(idx);
        } catch (const std::exception& e) {
            threw = true;
        }
        return RUN_CHECK(runner, name, threw, "indexToAudioData should throw for malformed/unsupported header bit depth");
    });

    // ------------------ operator== / operator!= tests ------------------
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

    runner.add("AudioIndex: writeIndexToFile outputs", [&runner]() -> bool {
        const std::string name = "AudioIndex: writeIndexToFile outputs";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            // use a known value that has varied digits
            cpp_int v = 0;
            // build a moderately-sized value: 0x1234_5678_9ABC_DEF0_1122
            v = cpp_int(0x12345678);
            v <<= 64;
            v |= cpp_int(0x9ABCDEF01122ULL);

            std::string prefix = "test_index_out"; // basename only; files are written into cpp/tests/indexes/
            AudioIndex::writeIndexToFile(v, std::string(), prefix);

            // Check base64 exist (new suffix .b64.txt)
            std::ifstream b64(std::string("cpp/tests/indexes/") + prefix + ".txt");
            ok &= RUN_CHECK(runner, name, bool(b64), "b64 file exists");
            // cleanup
            auto safe_rm = [&](const std::string& p) {
                try {
                    std::filesystem::remove(p);
                } catch (...) {
                }
            };
            safe_rm(std::string("cpp/tests/indexes/") + prefix + ".txt");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    // ------------------ Additional unit tests ------------------
    runner.add("AudioIndex: 16-bit edge values roundtrip", [&runner]() -> bool {
        const std::string     name = "AudioIndex: 16-bit edge values roundtrip";
        AudioIndex::AudioData audioData{};
        audioData.sample_rate  = 44100;
        audioData.bit_rate     = 16;
        audioData.num_channels = 1;
        audioData.audio_format = 1;
        audioData.num_frames   = 2;

        // samples: INT16_MIN, INT16_MAX
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
            ok &= RUN_CHECK(runner, name, audioData2.bit_rate == audioData.bit_rate, "bit_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.num_frames == audioData.num_frames, "num_frames match");
            ok &= RUN_CHECK(runner, name, audioData2.samples == audioData.samples, "samples content match");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    // New tests: header layout and sample byte-order
    runner.add("AudioIndex: header layout big-endian", [&runner]() -> bool {
        const std::string name = "AudioIndex: header layout big-endian";
        using boost::multiprecision::export_bits;
        bool ok = true;
        try {
            AudioIndex::AudioData audioData{};
            audioData.sample_rate  = 44100;
            audioData.bit_rate     = 16;
            audioData.num_channels = 1;
            audioData.audio_format = 1;
            audioData.num_frames   = 3;
            // create minimal samples (3 frames, 16-bit -> 6 bytes)
            audioData.samples.resize(audioData.num_frames * (audioData.bit_rate / 8));
            for (size_t i = 0; i < audioData.samples.size(); ++i) {
                audioData.samples[i] = static_cast<uint8_t>(i + 1);
            }

            auto idx = AudioIndex::audioDataToIndex(audioData);

            std::vector<uint8_t> bytes;
            boost::multiprecision::export_bits(idx, std::back_inserter(bytes), 8, true);

            const size_t HEADER_LEN = 16; // 4 + 2 + 2 + 8
            ok &= RUN_CHECK(runner, name, bytes.size() >= HEADER_LEN, "exported bytes contain header");
            if (bytes.size() >= HEADER_LEN) {
                // Build expected header in big-endian order
                std::vector<uint8_t> expected;
                uint32_t             sr = audioData.sample_rate;
                expected.push_back(static_cast<uint8_t>((sr >> 24) & 0xFF));
                expected.push_back(static_cast<uint8_t>((sr >> 16) & 0xFF));
                expected.push_back(static_cast<uint8_t>((sr >> 8) & 0xFF));
                expected.push_back(static_cast<uint8_t>((sr >> 0) & 0xFF));
                uint16_t br = audioData.bit_rate;
                expected.push_back(static_cast<uint8_t>((br >> 8) & 0xFF));
                expected.push_back(static_cast<uint8_t>((br >> 0) & 0xFF));
                uint16_t nc = audioData.num_channels;
                expected.push_back(static_cast<uint8_t>((nc >> 8) & 0xFF));
                expected.push_back(static_cast<uint8_t>((nc >> 0) & 0xFF));
                uint64_t nf = audioData.num_frames;
                for (int i = 7; i >= 0; --i) {
                    expected.push_back(static_cast<uint8_t>((nf >> (i * 8)) & 0xFF));
                }

                auto it    = bytes.end() - HEADER_LEN;
                bool match = true;
                for (size_t i = 0; i < HEADER_LEN; ++i) {
                    if (*(it + i) != expected[i]) {
                        match = false;
                        break;
                    }
                }
                ok &= RUN_CHECK(runner, name, match, "header bytes match expected big-endian layout");
            }
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("AudioIndex: extractAudioDataFromSamples byte-order", [&runner]() -> bool {
        const std::string name = "AudioIndex: extractAudioDataFromSamples byte-order";
        bool              ok   = true;
        // 16-bit sample ordering
        try {
            std::vector<int32_t> s16  = {0x1234};
            auto                 ad16 = AudioIndex::extractAudioDataFromSamples(s16, 44100, 16);
            ok &= RUN_CHECK(runner, name, ad16.samples.size() == 2, "16-bit sample produced 2 bytes");
            ok &= RUN_CHECK(runner, name, ad16.samples[0] == static_cast<uint8_t>(0x34), "16-bit LSB first byte");
            ok &= RUN_CHECK(runner, name, ad16.samples[1] == static_cast<uint8_t>(0x12), "16-bit MSB second byte");

            // 32-bit sample ordering
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

    runner.add("AudioIndex: indexToMetadata deterministic and valid", [&runner]() -> bool {
        const std::string name = "AudioIndex: indexToMetadata deterministic and valid";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            // Build a sample byte vector (non-empty) and construct a cpp_int (MSB-first)
            std::vector<uint8_t> bytes = {0x10, 0x20, 0x30, 0x41, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA,
                                          0xBB, 0xCC, 0xDD, 0xEE, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
            cpp_int              idx   = 0;
            for (uint8_t b : bytes) {
                idx <<= 8;
                idx |= cpp_int(static_cast<uint32_t>(b));
            }

            auto m1 = AudioIndex::indexToMetadata(idx);
            auto m2 = AudioIndex::indexToMetadata(idx);

            ok &= RUN_CHECK(runner, name, m1.genre == m2.genre, "genre deterministic");
            ok &= RUN_CHECK(runner, name, m1.artist == m2.artist, "artist deterministic");
            ok &= RUN_CHECK(runner, name, m1.album == m2.album, "album deterministic");
            ok &= RUN_CHECK(runner, name, m1.track == m2.track, "track deterministic");

            // Validate variable-length behavior: parts should be non-empty and
            // when concatenated they should recreate the URL-safe base64
            // representation of the original index bytes (no padding).
            static const char b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            std::string       b64str;
            b64str.reserve((bytes.size() * 8 + 5) / 6);
            uint32_t acc      = 0;
            int      acc_bits = 0;
            for (uint8_t byte : bytes) {
                acc = (acc << 8) | byte;
                acc_bits += 8;
                while (acc_bits >= 6) {
                    acc_bits -= 6;
                    uint8_t idx = static_cast<uint8_t>((acc >> acc_bits) & 0x3F);
                    b64str.push_back(b64_alpha[idx]);
                }
            }
            if (acc_bits > 0) {
                uint8_t idx = static_cast<uint8_t>((acc << (6 - acc_bits)) & 0x3F);
                b64str.push_back(b64_alpha[idx]);
            }

            ok &= RUN_CHECK(runner, name, !m1.genre.empty(), "genre non-empty");
            ok &= RUN_CHECK(runner, name, !m1.artist.empty(), "artist non-empty");
            ok &= RUN_CHECK(runner, name, !m1.album.empty(), "album non-empty");
            ok &= RUN_CHECK(runner, name, !m1.track.empty(), "track non-empty");

            auto valid_b64_chars = [&](const std::string& s) {
                for (char c : s) {
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
                        return false;
                    }
                }
                return true;
            };

            ok &= RUN_CHECK(runner, name, valid_b64_chars(m1.genre), "genre base64 chars valid");
            ok &= RUN_CHECK(runner, name, valid_b64_chars(m1.artist), "artist base64 chars valid");
            ok &= RUN_CHECK(runner, name, valid_b64_chars(m1.album), "album base64 chars valid");
            ok &= RUN_CHECK(runner, name, valid_b64_chars(m1.track), "track base64 chars valid");

            std::string recombined = m1.genre + m1.artist + m1.album + m1.track;
            ok &= RUN_CHECK(runner, name, recombined == b64str, "concatenation recreates base64 index");

            // Cover: should contain SVG markup
            ok &= RUN_CHECK(runner, name, !m1.cover.empty(), "cover non-empty");
            std::string cover_str(m1.cover.begin(), m1.cover.end());
            ok &= RUN_CHECK(runner, name, cover_str.find("<svg") != std::string::npos, "cover contains svg");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("IndexMetadata: string-overload deterministic and recomposition", [&runner]() -> bool {
        const std::string name = "IndexMetadata: string-overload deterministic and recomposition";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            // Build a deterministic byte array and a base64 string (URL-safe, no padding)
            std::vector<uint8_t> bytes = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};
            // encode to URL-safe base64 using same algorithm as production
            static const char b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            std::string       b64str;
            uint32_t          acc      = 0;
            int               acc_bits = 0;
            for (uint8_t byte : bytes) {
                acc = (acc << 8) | byte;
                acc_bits += 8;
                while (acc_bits >= 6) {
                    acc_bits -= 6;
                    uint8_t idx = static_cast<uint8_t>((acc >> acc_bits) & 0x3F);
                    b64str.push_back(b64_alpha[idx]);
                }
            }
            if (acc_bits > 0) {
                uint8_t idx = static_cast<uint8_t>((acc << (6 - acc_bits)) & 0x3F);
                b64str.push_back(b64_alpha[idx]);
            }

            // Call the string overload
            auto meta = IndexMetadata::extractMetadataFromIndex(b64str);

            // Basic assertions: parts non-empty and valid chars
            ok &= RUN_CHECK(runner, name, !meta.genre.empty(), "genre non-empty");
            ok &= RUN_CHECK(runner, name, !meta.artist.empty(), "artist non-empty");
            ok &= RUN_CHECK(runner, name, !meta.album.empty(), "album non-empty");
            ok &= RUN_CHECK(runner, name, !meta.track.empty(), "track non-empty");

            auto valid_b64_chars = [&](const std::string& s) {
                for (char c : s) {
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
                        return false;
                    }
                }
                return true;
            };

            ok &= RUN_CHECK(runner, name, valid_b64_chars(meta.genre), "genre base64 chars valid");
            ok &= RUN_CHECK(runner, name, valid_b64_chars(meta.artist), "artist base64 chars valid");
            ok &= RUN_CHECK(runner, name, valid_b64_chars(meta.album), "album base64 chars valid");
            ok &= RUN_CHECK(runner, name, valid_b64_chars(meta.track), "track base64 chars valid");

            std::string recombined = meta.genre + meta.artist + meta.album + meta.track;
            ok &= RUN_CHECK(runner, name, recombined == b64str, "concatenation recreates base64 index");

            // cover contains svg
            ok &= RUN_CHECK(runner, name, !meta.cover.empty(), "cover non-empty");
            std::string cover_str(meta.cover.begin(), meta.cover.end());
            ok &= RUN_CHECK(runner, name, cover_str.find("<svg") != std::string::npos, "cover contains svg");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("IndexMetadata: string-overload malformed input handling", [&runner]() -> bool {
        const std::string name = "IndexMetadata: string-overload malformed input handling";
        bool              ok   = true;
        try {
            // Create a valid small byte array and base64 string
            std::vector<uint8_t> bytes       = {0xDE, 0xAD, 0xBE, 0xEF};
            static const char    b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            std::string          clean_b64;
            uint32_t             acc      = 0;
            int                  acc_bits = 0;
            for (uint8_t byte : bytes) {
                acc = (acc << 8) | byte;
                acc_bits += 8;
                while (acc_bits >= 6) {
                    acc_bits -= 6;
                    uint8_t idx = static_cast<uint8_t>((acc >> acc_bits) & 0x3F);
                    clean_b64.push_back(b64_alpha[idx]);
                }
            }
            if (acc_bits > 0) {
                uint8_t idx = static_cast<uint8_t>((acc << (6 - acc_bits)) & 0x3F);
                clean_b64.push_back(b64_alpha[idx]);
            }

            // Inject some malformed characters into the base64 string
            std::string malformed = clean_b64;
            if (malformed.size() >= 2) {
                malformed.insert(1, "=");
                malformed.insert(malformed.size() - 1, "@");
            } else {
                malformed += "=@";
            }

            // Call the string overload with malformed input - expect an exception
            bool threw = false;
            try {
                auto meta = IndexMetadata::extractMetadataFromIndex(malformed);
                (void) meta; // silence unused in the non-throwing path
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            ok &= RUN_CHECK(runner, name, threw, "decoder throws on malformed base64 input");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("IndexMetadata: generateSvgCover color derivation", [&runner]() -> bool {
        const std::string name = "IndexMetadata: generateSvgCover color derivation";
        bool              ok   = true;
        try {
            std::vector<uint8_t> bytes = {0x12, 0x34, 0x56, 0x78};
            std::string          svg   = IndexMetadata::generateSvgCover(bytes, "t");
            // color computed from first three bytes: 0x12 0x34 0x56 -> hex 123456
            ok &= RUN_CHECK(runner, name, svg.find("#123456") != std::string::npos, "svg contains expected color #123456");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("IndexMetadata: generateSvgCover contains track text", [&runner]() -> bool {
        const std::string name = "IndexMetadata: generateSvgCover contains track text";
        bool              ok   = true;
        try {
            std::vector<uint8_t> bytes = {0xFF, 0xEE, 0xDD};
            std::string          track = "MyTrack";
            std::string          svg   = IndexMetadata::generateSvgCover(bytes, track);
            ok &= RUN_CHECK(runner, name, svg.find(track) != std::string::npos, "svg contains track text");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("AudioIndex: exportAudioDataToWav header correctness", [&runner]() -> bool {
        const std::string name = "AudioIndex: exportAudioDataToWav header correctness";
        bool              ok   = true;
        try {
            AudioIndex::AudioData audioData{};
            audioData.sample_rate  = 22050;
            audioData.bit_rate     = 16;
            audioData.num_channels = 1;
            audioData.audio_format = 1;
            audioData.num_frames   = 3;
            size_t data_bytes      = audioData.num_frames * audioData.num_channels * (audioData.bit_rate / 8);
            audioData.samples.resize(data_bytes);
            for (size_t i = 0; i < data_bytes; ++i) {
                audioData.samples[i] = static_cast<uint8_t>(i + 1);
            }

            TempFile tmp(make_temp_path("temp_export_header_test.wav"));
            AudioIndex::exportAudioDataToWav(audioData, tmp.path());

            std::ifstream in(tmp.path(), std::ios::binary);
            if (!in) {
                runner.failMsg(name, "failed to open written WAV file");
                return false;
            }
            std::vector<uint8_t> hdr(44);
            in.read(reinterpret_cast<char*>(hdr.data()), static_cast<std::streamsize>(hdr.size()));
            if (!in) {
                runner.failMsg(name, "failed to read WAV header bytes");
                return false;
            }

            ok &= RUN_CHECK(runner, name, hdr[0] == 'R' && hdr[1] == 'I' && hdr[2] == 'F' && hdr[3] == 'F', "RIFF tag");
            uint32_t file_size = static_cast<uint32_t>(hdr[4]) | (static_cast<uint32_t>(hdr[5]) << 8) | (static_cast<uint32_t>(hdr[6]) << 16) |
                                 (static_cast<uint32_t>(hdr[7]) << 24);
            uint32_t expected_file_size = 36U + static_cast<uint32_t>(audioData.samples.size());
            ok &= RUN_CHECK(runner, name, file_size == expected_file_size, "file size matches expected (36 + data bytes)");

            ok &= RUN_CHECK(runner, name, hdr[8] == 'W' && hdr[9] == 'A' && hdr[10] == 'V' && hdr[11] == 'E', "WAVE tag");
            ok &= RUN_CHECK(runner, name, hdr[12] == 'f' && hdr[13] == 'm' && hdr[14] == 't' && hdr[15] == ' ', "fmt chunk id");

            uint32_t fmt_size = static_cast<uint32_t>(hdr[16]) | (static_cast<uint32_t>(hdr[17]) << 8) | (static_cast<uint32_t>(hdr[18]) << 16) |
                                (static_cast<uint32_t>(hdr[19]) << 24);
            ok &= RUN_CHECK(runner, name, fmt_size == 16U, "fmt chunk size == 16");

            uint16_t audio_format = static_cast<uint16_t>(hdr[20]) | (static_cast<uint16_t>(hdr[21]) << 8);
            ok &= RUN_CHECK(runner, name, audio_format == audioData.audio_format, "audio format matches (PCM=1)");

            uint16_t num_channels = static_cast<uint16_t>(hdr[22]) | (static_cast<uint16_t>(hdr[23]) << 8);
            ok &= RUN_CHECK(runner, name, num_channels == audioData.num_channels, "num channels matches");

            uint32_t sample_rate = static_cast<uint32_t>(hdr[24]) | (static_cast<uint32_t>(hdr[25]) << 8) | (static_cast<uint32_t>(hdr[26]) << 16) |
                                   (static_cast<uint32_t>(hdr[27]) << 24);
            ok &= RUN_CHECK(runner, name, sample_rate == audioData.sample_rate, "sample rate matches");

            uint16_t bits_per_sample = static_cast<uint16_t>(hdr[34]) | (static_cast<uint16_t>(hdr[35]) << 8);
            ok &= RUN_CHECK(runner, name, bits_per_sample == audioData.bit_rate, "bits per sample matches bit_rate");

            ok &= RUN_CHECK(runner, name, hdr[36] == 'd' && hdr[37] == 'a' && hdr[38] == 't' && hdr[39] == 'a', "data chunk id");
            uint32_t data_size = static_cast<uint32_t>(hdr[40]) | (static_cast<uint32_t>(hdr[41]) << 8) | (static_cast<uint32_t>(hdr[42]) << 16) |
                                 (static_cast<uint32_t>(hdr[43]) << 24);
            ok &= RUN_CHECK(runner, name, data_size == static_cast<uint32_t>(audioData.samples.size()), "data chunk size matches samples size");

            // cleanup handled by TempFile destructor

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
        // Build audioData with small 16-bit samples whose MSB bytes are zero
        AudioIndex::AudioData audioData{};
        audioData.sample_rate  = 44100;
        audioData.bit_rate     = 16;
        audioData.num_channels = 1;
        audioData.audio_format = 1;
        audioData.num_frames   = 4;
        size_t bytes           = audioData.num_frames * audioData.num_channels * (audioData.bit_rate / 8);
        audioData.samples.resize(bytes);
        // samples: 1,2,3,4 -> little-endian bytes (LSB first), big-endian MSB will be zero
        for (size_t i = 0; i < audioData.num_frames; ++i) {
            auto   v                   = static_cast<int16_t>(i + 1);
            size_t off                 = i * 2;
            audioData.samples[off + 0] = static_cast<uint8_t>(v & 0xFF);
            audioData.samples[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        }

        // Clear any previous debug info
        AudioIndex::clearLastDebugInfo();

        bool ok = true;
        try {
            auto idx        = AudioIndex::audioDataToIndex(audioData);
            auto audioData2 = AudioIndex::indexToAudioData(idx);

            auto   dbg            = AudioIndex::getLastDebugInfo();
            size_t expected_bytes = audioData.num_frames * audioData.num_channels * (audioData.bit_rate / 8);
            ok &= RUN_CHECK(runner, name, dbg.export_expected_bytes == expected_bytes, "export_expected_bytes equals expected");
            ok &= RUN_CHECK(runner, name, dbg.export_pcm_bytes == expected_bytes, "export_pcm_bytes was padded to expected");
            ok &= RUN_CHECK(runner, name, audioData2.samples == audioData.samples, "samples round-trip exactly");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    // WAV edge-case tests: fmt chunk with extra bytes, odd-sized unknown chunk, and truncated file
    runner.add("AudioIndex: wav fmt chunk with extra bytes", [&runner]() -> bool {
        const std::string name = "AudioIndex: wav fmt chunk with extra bytes";
        bool              ok   = true;
        TempFile          tmp(make_temp_path("temp_fmt_extra.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }

            // Parameters
            uint16_t audio_format    = 1;
            uint16_t num_channels    = 1;
            uint32_t sample_rate     = 44100;
            uint16_t bits_per_sample = 16;
            uint32_t byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

            // payload
            std::vector<uint8_t> data      = {0x11, 0x22, 0x33, 0x44};
            auto                 data_size = static_cast<uint32_t>(data.size());

            uint32_t fmt_size = 18; // 2 extra bytes beyond canonical 16

            // Compute RIFF size = 4 (WAVE) + (8 + fmt_size) + (8 + data_size)
            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

            // write RIFF header
            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            // fmt chunk
            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

            // 16 canonical bytes
            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            // extra two bytes
            out.put(static_cast<char>(0x55));
            out.put(static_cast<char>(0x66));

            // data chunk
            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            // call extractor
            auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            ok &= RUN_CHECK(runner, name, ad.sample_rate == sample_rate, "sample rate matches");
            ok &= RUN_CHECK(runner, name, ad.bit_rate == bits_per_sample, "bit depth matches");
            ok &= RUN_CHECK(runner, name, ad.num_channels == num_channels, "num channels matches");
            ok &= RUN_CHECK(runner, name, ad.samples.size() == data_size, "data size matches");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        // cleanup handled by TempFile destructor
        return ok;
    });

    runner.add("AudioIndex: wav fmt variants (odd size, invalid byte_rate, extra bytes)", [&runner]() -> bool {
        const std::string name = "AudioIndex: wav fmt variants (odd size, invalid byte_rate, extra bytes)";
        bool              ok   = true;

        // Case A: fmt chunk odd size (17) with one extra byte - should be tolerated
        try {
            TempFile      tmp(make_temp_path("temp_fmt_odd17.wav"));
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav (fmt odd 17)");
                return false;
            }
            uint16_t             audio_format    = 1;
            uint16_t             num_channels    = 1;
            uint32_t             sample_rate     = 44100;
            uint16_t             bits_per_sample = 16;
            uint32_t             byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto                 block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));
            std::vector<uint8_t> data            = {0x11, 0x22};
            auto                 data_size       = static_cast<uint32_t>(data.size());

            uint32_t fmt_size  = 17; // odd
            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

            // canonical 16 bytes
            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            // one extra byte (odd fmt)
            out.put(static_cast<char>(0x7F));

            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            ok &= RUN_CHECK(runner, name, ad.sample_rate == sample_rate, "fmt odd: sample rate matches");
            ok &= RUN_CHECK(runner, name, ad.bit_rate == bits_per_sample, "fmt odd: bit depth matches");
            ok &= RUN_CHECK(runner, name, ad.num_channels == num_channels, "fmt odd: num channels matches");
            ok &= RUN_CHECK(runner, name, ad.samples.size() == data_size, "fmt odd: data size matches");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception (fmt odd): ") + e.what());
            ok = false;
        }

        // Case B: invalid byte_rate (zero) - extractor should not crash and should parse other fields
        try {
            TempFile      tmp(make_temp_path("temp_fmt_byte_rate0.wav"));
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav (byte_rate==0)");
                return false;
            }
            uint16_t             audio_format    = 1;
            uint16_t             num_channels    = 1;
            uint32_t             sample_rate     = 22050;
            uint16_t             bits_per_sample = 16;
            uint32_t             byte_rate       = 0; // invalid
            auto                 block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));
            std::vector<uint8_t> data            = {0x55, 0x66, 0x77};
            auto                 data_size       = static_cast<uint32_t>(data.size());

            uint32_t fmt_size  = 16;
            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            ok &= RUN_CHECK(runner, name, ad.sample_rate == sample_rate, "byte_rate0: sample rate matches");
            ok &= RUN_CHECK(runner, name, ad.bit_rate == bits_per_sample, "byte_rate0: bit depth matches");
            ok &= RUN_CHECK(runner, name, ad.num_channels == num_channels, "byte_rate0: num channels matches");
            ok &= RUN_CHECK(runner, name, ad.samples.size() == data_size, "byte_rate0: data size matches");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception (byte_rate0): ") + e.what());
            ok = false;
        }

        // Case C: fmt chunk odd size with multiple extra bytes (19) - should be tolerated
        try {
            TempFile      tmp(make_temp_path("temp_fmt_odd19.wav"));
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav (fmt odd 19)");
                return false;
            }
            uint16_t             audio_format    = 1;
            uint16_t             num_channels    = 2;
            uint32_t             sample_rate     = 48000;
            uint16_t             bits_per_sample = 16;
            uint32_t             byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto                 block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));
            std::vector<uint8_t> data            = {0xAA, 0xBB, 0xCC, 0xDD};
            auto                 data_size       = static_cast<uint32_t>(data.size());

            uint32_t fmt_size  = 19; // odd with multiple extra bytes
            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            // three extra bytes
            out.put(static_cast<char>(0x01));
            out.put(static_cast<char>(0x02));
            out.put(static_cast<char>(0x03));

            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            ok &= RUN_CHECK(runner, name, ad.sample_rate == sample_rate, "fmt odd19: sample rate matches");
            ok &= RUN_CHECK(runner, name, ad.bit_rate == bits_per_sample, "fmt odd19: bit depth matches");
            ok &= RUN_CHECK(runner, name, ad.num_channels == num_channels, "fmt odd19: num channels matches");
            ok &= RUN_CHECK(runner, name, ad.samples.size() == data_size, "fmt odd19: data size matches");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception (fmt odd19): ") + e.what());
            ok = false;
        }

        return ok;
    });

    runner.add("AudioIndex: wav odd-sized unknown chunk with padding", [&runner]() -> bool {
        const std::string name = "AudioIndex: wav odd-sized unknown chunk with padding";
        bool              ok   = true;
        TempFile          tmp(make_temp_path("temp_odd_junk.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }

            uint16_t audio_format    = 1;
            uint16_t num_channels    = 1;
            uint32_t sample_rate     = 22050;
            uint16_t bits_per_sample = 16;
            uint32_t byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

            std::vector<uint8_t> data      = {0xAA, 0xBB, 0xCC, 0xDD};
            auto                 data_size = static_cast<uint32_t>(data.size());

            uint32_t fmt_size = 16;
            // unknown chunk size odd (3)
            uint32_t junk_size = 3;

            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + junk_size + 1) + (8 + data_size); // include pad byte for junk

            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            // fmt chunk
            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));
            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            // JUNK chunk (odd length)
            out.write("JUNK", 4);
            out.put(static_cast<char>(junk_size & 0xFF));
            out.put(static_cast<char>((junk_size >> 8) & 0xFF));
            out.put(static_cast<char>((junk_size >> 16) & 0xFF));
            out.put(static_cast<char>((junk_size >> 24) & 0xFF));
            // 3 bytes of junk
            out.put(static_cast<char>(0x01));
            out.put(static_cast<char>(0x02));
            out.put(static_cast<char>(0x03));
            // pad byte because chunk size is odd
            out.put(static_cast<char>(0x00));

            // data chunk
            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            ok &= RUN_CHECK(runner, name, ad.sample_rate == sample_rate, "sample rate matches");
            ok &= RUN_CHECK(runner, name, ad.bit_rate == bits_per_sample, "bit depth matches");
            ok &= RUN_CHECK(runner, name, ad.num_channels == num_channels, "num channels matches");
            ok &= RUN_CHECK(runner, name, ad.samples.size() == data_size, "data size matches");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        // cleanup handled by TempFile destructor
        return ok;
    });

    runner.add("AudioIndex: wav truncated file throws", [&runner]() -> bool {
        const std::string name  = "AudioIndex: wav truncated file throws";
        bool              threw = false;
        TempFile          tmp(make_temp_path("temp_truncated.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }
            // write a deliberately truncated RIFF header (incomplete WAVE)
            out.write("RIFF", 4);
            out.put(static_cast<char>(0));
            out.put(static_cast<char>(0));
            out.put(static_cast<char>(0));
            out.put(static_cast<char>(0));
            out.write("WA", 2); // incomplete 'WAVE'
            out.close();

            try {
                auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            } catch (const std::exception& e) {
                threw = true;
            }
        } catch (...) {
            threw = true;
        }
        // cleanup handled by TempFile destructor
        return RUN_CHECK(runner, name, threw, "truncated wav should cause extractor to throw or fail");
    });

    // Additional negative WAV tests
    runner.add("AudioIndex: wav unsupported bitsPerSample throws", [&runner]() -> bool {
        const std::string name  = "AudioIndex: wav unsupported bitsPerSample throws";
        bool              threw = false;
        TempFile          tmp(make_temp_path("temp_unsupported_bps.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }

            uint16_t audio_format = 1;
            uint16_t num_channels = 1;
            uint32_t sample_rate  = 44100;
            // unsupported bits per sample (7)
            uint16_t bits_per_sample = 7;
            uint32_t byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

            std::vector<uint8_t> data      = {0x01, 0x02};
            auto                 data_size = static_cast<uint32_t>(data.size());

            // riff size
            uint32_t fmt_size  = 16;
            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            try {
                auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            } catch (const std::exception& e) {
                threw = true;
            }
        } catch (...) {
            threw = true;
        }
        // cleanup handled by TempFile destructor
        return RUN_CHECK(runner, name, threw, "unsupported bitsPerSample should cause extractor to throw or fail");
    });

    runner.add("AudioIndex: wav malformed headers throw", [&runner]() -> bool {
        const std::string name = "AudioIndex: wav malformed headers throw";
        bool              ok   = true;

        // Case A: bits_per_sample == 0
        {
            TempFile tmp(make_temp_path("temp_malformed_bps0.wav"));
            try {
                std::ofstream out(tmp.path(), std::ios::binary);
                if (!out) {
                    runner.failMsg(name, "failed to create temp wav (bps==0)");
                    return false;
                }

                uint16_t audio_format    = 1;
                uint16_t num_channels    = 1;
                uint32_t sample_rate     = 44100;
                uint16_t bits_per_sample = 0; // malformed
                uint32_t byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
                auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

                std::vector<uint8_t> data      = {0x01, 0x02};
                auto                 data_size = static_cast<uint32_t>(data.size());

                uint32_t fmt_size  = 16;
                uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

                out.write("RIFF", 4);
                out.put(static_cast<char>(riff_size & 0xFF));
                out.put(static_cast<char>((riff_size >> 8) & 0xFF));
                out.put(static_cast<char>((riff_size >> 16) & 0xFF));
                out.put(static_cast<char>((riff_size >> 24) & 0xFF));
                out.write("WAVE", 4);

                out.write("fmt ", 4);
                out.put(static_cast<char>(fmt_size & 0xFF));
                out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
                out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
                out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

                out.put(static_cast<char>(audio_format & 0xFF));
                out.put(static_cast<char>((audio_format >> 8) & 0xFF));
                out.put(static_cast<char>(num_channels & 0xFF));
                out.put(static_cast<char>((num_channels >> 8) & 0xFF));
                out.put(static_cast<char>(sample_rate & 0xFF));
                out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
                out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
                out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
                out.put(static_cast<char>(byte_rate & 0xFF));
                out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
                out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
                out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
                out.put(static_cast<char>(block_align & 0xFF));
                out.put(static_cast<char>((block_align >> 8) & 0xFF));
                out.put(static_cast<char>(bits_per_sample & 0xFF));
                out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

                out.write("data", 4);
                out.put(static_cast<char>(data_size & 0xFF));
                out.put(static_cast<char>((data_size >> 8) & 0xFF));
                out.put(static_cast<char>((data_size >> 16) & 0xFF));
                out.put(static_cast<char>((data_size >> 24) & 0xFF));
                out.write(reinterpret_cast<const char*>(data.data()), data.size());
                out.close();

                bool threw = false;
                try {
                    auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
                } catch (...) {
                    threw = true;
                }
                ok &= RUN_CHECK(runner, name, threw, "bits_per_sample == 0 should cause extractor to throw");
            } catch (const std::exception& e) {
                runner.failMsg(name, std::string("exception: ") + e.what());
                ok = false;
            }
        }

        // Case B: num_channels == 0
        {
            TempFile tmp(make_temp_path("temp_malformed_nc0.wav"));
            try {
                std::ofstream out(tmp.path(), std::ios::binary);
                if (!out) {
                    runner.failMsg(name, "failed to create temp wav (nc==0)");
                    return false;
                }

                uint16_t audio_format    = 1;
                uint16_t num_channels    = 0; // malformed
                uint32_t sample_rate     = 44100;
                uint16_t bits_per_sample = 16;
                uint32_t byte_rate       = sample_rate * (num_channels) * (bits_per_sample / 8);
                auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

                std::vector<uint8_t> data      = {0x01, 0x02};
                auto                 data_size = static_cast<uint32_t>(data.size());

                uint32_t fmt_size  = 16;
                uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

                out.write("RIFF", 4);
                out.put(static_cast<char>(riff_size & 0xFF));
                out.put(static_cast<char>((riff_size >> 8) & 0xFF));
                out.put(static_cast<char>((riff_size >> 16) & 0xFF));
                out.put(static_cast<char>((riff_size >> 24) & 0xFF));
                out.write("WAVE", 4);

                out.write("fmt ", 4);
                out.put(static_cast<char>(fmt_size & 0xFF));
                out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
                out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
                out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

                out.put(static_cast<char>(audio_format & 0xFF));
                out.put(static_cast<char>((audio_format >> 8) & 0xFF));
                out.put(static_cast<char>(num_channels & 0xFF));
                out.put(static_cast<char>((num_channels >> 8) & 0xFF));
                out.put(static_cast<char>(sample_rate & 0xFF));
                out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
                out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
                out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
                out.put(static_cast<char>(byte_rate & 0xFF));
                out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
                out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
                out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
                out.put(static_cast<char>(block_align & 0xFF));
                out.put(static_cast<char>((block_align >> 8) & 0xFF));
                out.put(static_cast<char>(bits_per_sample & 0xFF));
                out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

                out.write("data", 4);
                out.put(static_cast<char>(data_size & 0xFF));
                out.put(static_cast<char>((data_size >> 8) & 0xFF));
                out.put(static_cast<char>((data_size >> 16) & 0xFF));
                out.put(static_cast<char>((data_size >> 24) & 0xFF));
                out.write(reinterpret_cast<const char*>(data.data()), data.size());
                out.close();

                bool threw = false;
                try {
                    auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
                } catch (...) {
                    threw = true;
                }
                ok &= RUN_CHECK(runner, name, threw, "num_channels == 0 should cause extractor to throw");
            } catch (const std::exception& e) {
                runner.failMsg(name, std::string("exception: ") + e.what());
                ok = false;
            }
        }

        // Case C: missing fmt chunk (only data chunk present)
        {
            TempFile tmp(make_temp_path("temp_malformed_no_fmt.wav"));
            try {
                std::ofstream out(tmp.path(), std::ios::binary);
                if (!out) {
                    runner.failMsg(name, "failed to create temp wav (no fmt)");
                    return false;
                }

                std::vector<uint8_t> data      = {0xDE, 0xAD, 0xBE, 0xEF};
                auto                 data_size = static_cast<uint32_t>(data.size());
                uint32_t             riff_size = 4 + (8 + data_size);

                out.write("RIFF", 4);
                out.put(static_cast<char>(riff_size & 0xFF));
                out.put(static_cast<char>((riff_size >> 8) & 0xFF));
                out.put(static_cast<char>((riff_size >> 16) & 0xFF));
                out.put(static_cast<char>((riff_size >> 24) & 0xFF));
                out.write("WAVE", 4);

                out.write("data", 4);
                out.put(static_cast<char>(data_size & 0xFF));
                out.put(static_cast<char>((data_size >> 8) & 0xFF));
                out.put(static_cast<char>((data_size >> 16) & 0xFF));
                out.put(static_cast<char>((data_size >> 24) & 0xFF));
                out.write(reinterpret_cast<const char*>(data.data()), data.size());
                out.close();

                bool threw = false;
                try {
                    auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
                } catch (...) {
                    threw = true;
                }
                ok &= RUN_CHECK(runner, name, threw, "missing fmt chunk should cause extractor to throw");
            } catch (const std::exception& e) {
                runner.failMsg(name, std::string("exception: ") + e.what());
                ok = false;
            }
        }

        return ok;
    });

    runner.add("AudioIndex: wav fmt chunk too small throws", [&runner]() -> bool {
        const std::string name  = "AudioIndex: wav fmt chunk too small throws";
        bool              threw = false;
        TempFile          tmp(make_temp_path("temp_fmt_small.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }

            // write RIFF and a fmt chunk with size 10 (<16), no data chunk
            out.write("RIFF", 4);
            out.put(static_cast<char>(0));
            out.put(static_cast<char>(0));
            out.put(static_cast<char>(0));
            out.put(static_cast<char>(0));
            out.write("WAVE", 4);
            out.write("fmt ", 4);
            uint32_t small = 10;
            out.put(static_cast<char>(small & 0xFF));
            out.put(static_cast<char>((small >> 8) & 0xFF));
            out.put(static_cast<char>((small >> 16) & 0xFF));
            out.put(static_cast<char>((small >> 24) & 0xFF));
            // write 10 arbitrary bytes to satisfy the small chunk
            for (int i = 0; i < 10; ++i) {
                out.put(static_cast<char>(i));
            }
            out.close();

            try {
                auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            } catch (const std::exception& e) {
                threw = true;
            }
        } catch (...) {
            threw = true;
        }
        // cleanup handled by TempFile destructor
        return RUN_CHECK(runner, name, threw, "fmt chunk too small should cause extractor to fail/throw because no data chunk will be found");
    });

    runner.add("AudioIndex: wav data chunk declared larger than actual throws", [&runner]() -> bool {
        const std::string name  = "AudioIndex: wav data chunk declared larger than actual throws";
        bool              threw = false;
        TempFile          tmp(make_temp_path("temp_data_mismatch.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }

            uint16_t audio_format    = 1;
            uint16_t num_channels    = 1;
            uint32_t sample_rate     = 8000;
            uint16_t bits_per_sample = 16;
            uint32_t byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

            std::vector<uint8_t> data          = {0xDE, 0xAD, 0xBE, 0xEF};
            uint32_t             declared_size = 10; // declare larger than actual
            auto                 actual_size   = static_cast<uint32_t>(data.size());

            uint32_t fmt_size  = 16;
            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + declared_size);

            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));
            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            out.write("data", 4);
            out.put(static_cast<char>(declared_size & 0xFF));
            out.put(static_cast<char>((declared_size >> 8) & 0xFF));
            out.put(static_cast<char>((declared_size >> 16) & 0xFF));
            out.put(static_cast<char>((declared_size >> 24) & 0xFF));
            // write only actual_size bytes
            out.write(reinterpret_cast<const char*>(data.data()), actual_size);
            out.close();

            try {
                auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            } catch (const std::exception& e) {
                threw = true;
            }

        } catch (...) {
            threw = true;
        }
        // cleanup handled by TempFile destructor
        return RUN_CHECK(runner, name, threw, "declared data chunk larger than actual should cause extractor to fail/throw");
    });

    // ------------------ Integration: round-trip Test Audio files ------------------
    runner.add("AudioIndex: round-trip test audio directory", [&runner]() -> bool {
        const std::string name = "AudioIndex: round-trip test audio directory";
        namespace fs           = std::filesystem;
        // Locate the Test Audio directory relative to the current working directory.
        auto locate_in_dir = [&]() -> fs::path {
            fs::path cur = fs::current_path();
            for (int i = 0; i < 6; ++i) {
                fs::path cand = cur / "cpp" / "tests" / "Test Audio";
                if (fs::exists(cand) && fs::is_directory(cand)) {
                    return cand;
                }
                if (cur.has_parent_path()) {
                    cur = cur.parent_path();
                } else {
                    break;
                }
            }
            return {};
        };

        fs::path inDir  = locate_in_dir();
        fs::path outDir = inDir / "Outputs";
        bool     ok     = true;
        try {
            if (!fs::exists(inDir) || !fs::is_directory(inDir)) {
                std::cout << "  [SKIP] " << name << " — tests/Test Audio directory not found\n";
                return true; // skip if test data not present
            }
            if (!fs::exists(outDir)) {
                fs::create_directories(outDir);
            }

            for (const auto& ent : fs::directory_iterator(inDir)) {
                if (!ent.is_regular_file()) {
                    continue;
                }
                const auto& p = ent.path();
                if (p.extension() != ".wav" && p.extension() != ".WAV") {
                    continue;
                }
                std::vector<int32_t> samples;
                int                  sr = 0;

                // Log original properties
                std::ostringstream orig;
                orig << "FILE: " << p.string() << " | sr=" << sr << " | frames=" << samples.size() << " | bytes=" << (samples.size() * 2);
                log_now(orig.str());

                // Load, round-trip, and verify
                log_now("Extracting Audio Data from: " + p.string());
                auto originalData = AudioIndex::extractAudioDataFromAudioFile(p.string());

                log_now("Converting Audio Data to Index for: " + p.string());
                auto idx = AudioIndex::audioDataToIndex(originalData);

                // Write index representations (b256)
                try {
                    std::string stem = p.stem().string();
                    AudioIndex::writeIndexToFile(idx, std::string(), stem);
                    log_now(std::string("WROTE INDEX REPRS: cpp/tests/indexes/") + stem);
                } catch (const std::exception& e) {
                    log_now(std::string("WARN: failed to write index representations for: ") + p.string() + " err=" + e.what());
                } catch (...) {
                    log_now(std::string("WARN: failed to write index representations for: ") + p.string());
                }

                log_now("Reconstructing Audio Data from Index for: " + p.string());
                auto reconstructedData = AudioIndex::indexToAudioData(idx);

                // Log debug stats if available
                try {
                    auto               dbg = AudioIndex::getLastDebugInfo();
                    std::ostringstream dbgss;
                    dbgss << "DEBUG: " << p.string() << " | import_bytes=" << dbg.import_pcm_bytes << " expected_import=" << dbg.import_expected_bytes
                          << " | export_bytes=" << dbg.export_pcm_bytes << " expected_export=" << dbg.export_expected_bytes
                          << " | ms_import=" << dbg.audioDataToIndexMs << " ms_export=" << dbg.indexToAudioDataMs;
                    log_now(dbgss.str());
                } catch (...) {
                }

                // Fidelity checks: ensure reconstructed audio matches original metadata and payload
                if (reconstructedData.sample_rate != originalData.sample_rate) {
                    runner.failMsg(name, std::string("sample_rate mismatch for: ") + p.string());
                    ok = false;
                }
                if (reconstructedData.bit_rate != originalData.bit_rate) {
                    runner.failMsg(name, std::string("bit_rate mismatch for: ") + p.string());
                    ok = false;
                }
                if (reconstructedData.num_channels != originalData.num_channels) {
                    runner.failMsg(name, std::string("num_channels mismatch for: ") + p.string());
                    ok = false;
                }
                if (reconstructedData.num_frames != originalData.num_frames) {
                    runner.failMsg(name, std::string("num_frames mismatch for: ") + p.string());
                    ok = false;
                }
                if (reconstructedData.samples.size() != originalData.samples.size()) {
                    runner.failMsg(name, std::string("samples byte-size mismatch for: ") + p.string());
                    ok = false;
                } else if (reconstructedData.samples != originalData.samples) {
                    // compute a simple diff summary (count differing bytes)
                    size_t diffs = 0;
                    for (size_t i = 0; i < originalData.samples.size(); ++i) {
                        if (originalData.samples[i] != reconstructedData.samples[i]) {
                            ++diffs;
                        }
                    }
                    std::ostringstream ss;
                    ss << "sample payload differs (" << diffs << " bytes) for: " << p.string();
                    runner.failMsg(name, ss.str());
                    log_now(std::string("FILE DIFF: ") + ss.str());
                    ok = false;
                }

                // Log reconstructed properties
                std::ostringstream recon;
                recon << "RECON: " << p.string() << " | sr=" << reconstructedData.sample_rate << " | frames=" << reconstructedData.num_frames
                      << " | bytes=" << reconstructedData.samples.size();
                log_now(recon.str());

                fs::path outPath = outDir / (p.stem().string() + std::string("_recon.wav"));
                try {
                    log_now("Writing Reconstructed Audio Data to: " + outPath.string());
                    AudioIndex::exportAudioDataToWav(reconstructedData, outPath.string());
                } catch (const std::exception& e) {
                    runner.failMsg(name, std::string("failed to write recon for: ") + p.string());
                    ok = false;
                    continue;
                }
            }
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            return false;
        }
        return RUN_CHECK(runner, name, ok, "round-trip all wav files in tests/Test Audio");
    });

    // open log file in the repo root (or current dir) as test_run.log
    try {
        // Place the log under cpp/tests for easier discovery
        g_log.open("cpp/tests/test_run.log", std::ios::out | std::ios::app);
        log_now(std::string("TEST RUN START"));
    } catch (...) {
    }

    std::string filter;
    if (argc > 1) {
        filter = argv[1];
    }

    runner.runAll(filter);

    log_now(std::string("TEST RUN END: ") + std::to_string(runner.passed) + " passed, " + std::to_string(runner.failed) + " failed");
    if (g_log) {
        g_log.close();
    }
    return (runner.failed == 0) ? 0 : 1;
}
