#include "AudioIndex.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <map>
#include <functional>
#include <sstream>
#include <cstring>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <fstream>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace AudioBabel;

// Global log file used by the test runner and integration tests
static std::ofstream g_log;

static void log_now(const std::string& msg, bool printToConsole = false) {
    if (!g_log) return;
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    g_log << "[" << std::put_time(std::localtime(&tt), "%F %T") << "] " << msg << std::endl;
    g_log.flush();

    if (printToConsole) {
        std::cout << "[" << std::put_time(std::localtime(&tt), "%F %T") << "] " << msg << std::endl;
    }
}

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
    int passed = 0;
    int failed = 0;
    std::map<std::string, std::function<bool()>> tests;

    void add(const std::string& name, const std::function<bool()>& fn) {
        tests[name] = fn;
    }

    /**
     * Add a test to the runner.
     * @param name Human-readable test name shown in console output.
     * @param fn   Callable returning true on success, false on failure.
     */

    static bool approxEqual(double a, double b, double tol = 1e-6) {
        return std::fabs(a - b) <= tol;
    }

    static bool vecNotEmpty(const std::vector<int32_t>& v) { return !v.empty(); }

    void failMsg(const std::string& test, const std::string& msg) {
        std::cout << "  ✗ " << test << " — " << msg << std::endl;
        ++failed;
    }

    /**
     * Called by tests to record an individual failure message. Increments
     * the failure counter and prints a short failure line.
     */

    void passMsg(const std::string& test) {
        std::cout << "  ✓ " << test << std::endl;
        ++passed;
    }

    /**
     * Called by the harness when a test completes successfully. Prints
     * a short success line and increments the pass counter.
     */

    bool runOne(const std::string& name) {
        auto it = tests.find(name);
        if (it == tests.end()) {
            std::cout << "Test not found: " << name << std::endl;
            return false;
        }
        // No spinner: run the test and measure duration. Print one line with
        // the elapsed time and the final result. To avoid double-counting
        // assertion failures (which call runner.failMsg), capture the
        // failure count before/after running the test.
        size_t failed_before = static_cast<size_t>(failed);
        bool ok = false;
        std::string exceptionMsg;
    auto t0 = std::chrono::steady_clock::now();
    log_now(std::string("START TEST: ") + name);
        try {
            ok = it->second();
        } catch (const std::exception& e) {
            ok = false;
            exceptionMsg = std::string("exception: ") + e.what();
        } catch (...) {
            ok = false;
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
            if (!filter.empty() && kv.first.find(filter) == std::string::npos) continue;
            // Log the start of the test
            log_now(std::string("============== RUNNING TEST: [") + kv.first + "] ==============");
            std::cout << "============== RUNNING TEST: [" << kv.first << "] ==============" << std::endl;
            runOne(kv.first);
        }
        std::cout << "\nSummary: " << passed << " passed, " << failed << " failed" << std::endl;
    }
};

// Helpers for assertions used inside tests
static bool CHECK(bool cond, TestRunner& runner, const std::string& test, const std::string& msg="") {
    if (!cond) {
        runner.failMsg(test, msg.empty() ? "check failed" : msg);
        return false;
    }
    return true;
}

// Shared helper: run CHECK and print per-assertion status for a named test
static bool RUN_CHECK(TestRunner& runner, const std::string& testName, bool cond, const std::string& msg) {
    bool ok = CHECK(cond, runner, testName, msg);
    if (ok) std::cout << "  [OK]   " << msg << std::endl;
    else std::cout << "  [FAIL] " << msg << std::endl;
    return ok;
}

int main(int argc, char** argv) {
    TestRunner runner;
    // Register split AudioIndex unit tests

    // -- Unit tests for AudioIndex basic behavior
    runner.add("AudioIndex: extractAudioDataFromSamples", [&runner]() -> bool {
        const std::string name = "AudioIndex: extractAudioDataFromSamples";
        std::vector<int32_t> samples;
        for (int i = 0; i < 10; ++i) samples.push_back((i % 2 == 0) ? 1000 : -1000);
        auto audioData = AudioIndex::extractAudioDataFromSamples(samples, 8000, 16);

        bool ok = true;
        ok &= RUN_CHECK(runner, name, audioData.sample_rate == 8000, "sample_rate");
        ok &= RUN_CHECK(runner, name, audioData.bit_rate == 16, "bit_rate");
        ok &= RUN_CHECK(runner, name, audioData.num_channels == 1, "num_channels");
        ok &= RUN_CHECK(runner, name, audioData.num_frames == samples.size(), "num_frames");
        ok &= RUN_CHECK(runner, name, audioData.samples.size() == samples.size() * (size_t)(audioData.bit_rate/8), "samples byte length");
        return ok;
    });

    runner.add("AudioIndex: audioData -> index -> audioData roundtrip", [&runner]() -> bool {
        const std::string name = "AudioIndex: audioData -> index -> audioData roundtrip";
        std::vector<int32_t> samples = {0, 12345, -12345, 30000, -30000};
        auto audioData = AudioIndex::extractAudioDataFromSamples(samples, 44100, 16);
        auto idx = AudioIndex::audioDataToIndex(audioData);
        auto audioData2 = AudioIndex::indexToAudioData(idx);

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
        const std::string name = "AudioIndex: fromAudioSamples and getters";
        std::vector<int32_t> samples(44100); // 1 second of silence at 44.1k
        for (size_t i = 0; i < samples.size(); ++i) samples[i] = 0;
        auto ai = AudioIndex::fromAudioSamples(samples, 44100, 16);

        bool ok = true;
        ok &= RUN_CHECK(runner, name, ai.getSampleRate() == 44100, "getSampleRate");
        ok &= RUN_CHECK(runner, name, ai.getBitDepth() == 16, "getBitDepth");
        ok &= RUN_CHECK(runner, name, TestRunner::approxEqual(ai.getDuration(), 1.0, 1e-6), "getDuration ~ 1s");
        return ok;
    });

    runner.add("AudioIndex: exportAudioDataToWav and read back", [&runner]() -> bool {
        const std::string name = "AudioIndex: exportAudioDataToWav and read back";
        std::vector<int32_t> samples = {0, 1000, -1000, 2000, -2000};
        auto audioData = AudioIndex::extractAudioDataFromSamples(samples, 22050, 16);
        std::string tmpPath = "./temp_test.wav";
        bool ok = true;
        try {
            AudioIndex::exportAudioDataToWav(audioData, tmpPath);
            auto audioData2 = AudioIndex::extractAudioDataFromAudioFile(tmpPath);
            ok &= RUN_CHECK(runner, name, audioData2.sample_rate == audioData.sample_rate, "sample_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.bit_rate == audioData.bit_rate, "bit_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.num_channels == audioData.num_channels, "num_channels match");
            ok &= RUN_CHECK(runner, name, audioData2.num_frames == audioData.num_frames, "num_frames match");
            ok &= RUN_CHECK(runner, name, audioData2.samples == audioData.samples, "samples match");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        // best-effort cleanup
        try { std::remove(tmpPath.c_str()); } catch(...) {}
        return ok;
    });

    // ------------------ Negative / edge-case tests ------------------
    runner.add("AudioIndex: unsupported bit depth throws", [&runner]() -> bool {
        const std::string name = "AudioIndex: unsupported bit depth throws";
        std::vector<int32_t> samples = {0,1,2};
        auto audioData = AudioIndex::extractAudioDataFromSamples(samples, 8000, 12); // 12-bit unsupported
        bool threw = false;
        try {
            auto idx = AudioIndex::audioDataToIndex(audioData);
        } catch (const std::exception& e) {
            threw = true;
        }
        return RUN_CHECK(runner, name, threw, "audioDataToIndex should throw for unsupported bit depth");
    });

    runner.add("AudioIndex: empty samples roundtrip", [&runner]() -> bool {
        const std::string name = "AudioIndex: empty samples roundtrip";
        std::vector<int32_t> samples; // empty
        auto audioData = AudioIndex::extractAudioDataFromSamples(samples, 48000, 16);
        bool ok = true;
        ok &= RUN_CHECK(runner, name, audioData.num_frames == 0, "num_frames==0");
        try {
            auto idx = AudioIndex::audioDataToIndex(audioData);
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
        const std::string name = "AudioIndex: zero sampleRate duration is zero";
        std::vector<int32_t> samples(10, 1000);
        auto ai = AudioIndex::fromAudioSamples(samples, 0, 16);
        bool ok = RUN_CHECK(runner, name, TestRunner::approxEqual(ai.getDuration(), 0.0, 1e-12), "duration==0 when sampleRate==0");
        return ok;
    });

    runner.add("AudioIndex: malformed header bit depth rejected", [&runner]() -> bool {
        const std::string name = "AudioIndex: malformed header bit depth rejected";
        using boost::multiprecision::cpp_int;
        
        // Build a header with unsupported bit depth (7)
        std::vector<uint8_t> header_buf;
        uint32_t sr = 44100;
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
        for (int i = 7; i >= 0; --i) header_buf.push_back(static_cast<uint8_t>((nf >> (i*8)) & 0xFF));

        cpp_int header_int = 0;
        for (uint8_t b : header_buf) { header_int <<= 8; header_int |= cpp_int(uint32_t(b)); }
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
        const std::string name = "AudioIndex: operator== equal objects";
        std::vector<int32_t> samples = {100, -100, 200, -200};
        auto a = AudioIndex::fromAudioSamples(samples, 44100, 16);
        auto b = AudioIndex::fromAudioSamples(samples, 44100, 16);
        bool ok = true;
        ok &= RUN_CHECK(runner, name, a == b, "a == b");
        ok &= RUN_CHECK(runner, name, !(a != b), "!(a != b)");
        return ok;
    });

    runner.add("AudioIndex: operator== different samples unequal", [&runner]() -> bool {
        const std::string name = "AudioIndex: operator== different samples unequal";
        std::vector<int32_t> s1 = {0,1,2,3};
        std::vector<int32_t> s2 = {0,1,2,4};
        auto a = AudioIndex::fromAudioSamples(s1, 44100, 16);
        auto b = AudioIndex::fromAudioSamples(s2, 44100, 16);
        bool ok = true;
        ok &= RUN_CHECK(runner, name, a != b, "a != b for different samples");
        ok &= RUN_CHECK(runner, name, !(a == b), "!(a == b)");
        return ok;
    });

    runner.add("AudioIndex: operator== different sampleRate unequal", [&runner]() -> bool {
        const std::string name = "AudioIndex: operator== different sampleRate unequal";
        std::vector<int32_t> samples = {10,20,30,40};
        auto a = AudioIndex::fromAudioSamples(samples, 44100, 16);
        auto b = AudioIndex::fromAudioSamples(samples, 22050, 16);
        bool ok = true;
        ok &= RUN_CHECK(runner, name, a != b, "a != b for different sample rates");
        ok &= RUN_CHECK(runner, name, !(a == b), "!(a == b)");
        return ok;
    });

    // ------------------ Multi-channel and bit-depth round-trip tests ------------------
    runner.add("AudioIndex: stereo 16-bit roundtrip", [&runner]() -> bool {
        const std::string name = "AudioIndex: stereo 16-bit roundtrip";
        AudioIndex::AudioData audioData{};
        audioData.sample_rate = 44100;
        audioData.bit_rate = 16;
        audioData.num_channels = 2;
        audioData.audio_format = 1;
        audioData.num_frames = 4; // 4 frames, interleaved L,R

        // build interleaved samples: frames: (1000,-1000), (2000,-2000), ...
        std::vector<int16_t> left = {1000, 2000, 3000, 4000};
        std::vector<int16_t> right = {-1000, -2000, -3000, -4000};
        size_t bytes = audioData.num_frames * audioData.num_channels * (audioData.bit_rate/8);
        audioData.samples.resize(bytes);
        for (size_t i=0;i<audioData.num_frames;i++){
            int16_t l = left[i];
            int16_t r = right[i];
            size_t off = i * 2 * 2; // frame index * channels * bytes_per_sample
            audioData.samples[off + 0] = static_cast<uint8_t>(l & 0xFF);
            audioData.samples[off + 1] = static_cast<uint8_t>((l >> 8) & 0xFF);
            audioData.samples[off + 2] = static_cast<uint8_t>(r & 0xFF);
            audioData.samples[off + 3] = static_cast<uint8_t>((r >> 8) & 0xFF);
        }

        bool ok = true;
        try {
            auto idx = AudioIndex::audioDataToIndex(audioData);
            auto audioData2 = AudioIndex::indexToAudioData(idx);
            ok &= RUN_CHECK(runner, name, audioData2.sample_rate == audioData.sample_rate, "sample_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.bit_rate == audioData.bit_rate, "bit_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.num_channels == audioData.num_channels, "num_channels match");
            ok &= RUN_CHECK(runner, name, audioData2.num_frames == audioData.num_frames, "num_frames match");
            ok &= RUN_CHECK(runner, name, audioData2.samples == audioData.samples, "samples content match");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });


    runner.add("AudioIndex: stereo 32-bit roundtrip", [&runner]() -> bool {
        const std::string name = "AudioIndex: stereo 32-bit roundtrip";
        AudioIndex::AudioData audioData{};
        audioData.sample_rate = 48000;
        audioData.bit_rate = 32;
        audioData.num_channels = 2;
        audioData.audio_format = 1;
        audioData.num_frames = 3; // 3 frames

        std::vector<int32_t> left = {100000, 200000, -300000};
        std::vector<int32_t> right = {-100000, -200000, 300000};
        size_t bytes = audioData.num_frames * audioData.num_channels * (audioData.bit_rate/8);
        audioData.samples.resize(bytes);
        for (size_t i=0;i<audioData.num_frames;i++){
            int32_t l = left[i];
            int32_t r = right[i];
            size_t off = i * 2 * 4; // frame * channels * bytes_per_sample
            for (size_t b=0;b<4;b++) audioData.samples[off + b] = static_cast<uint8_t>((l >> (8*b)) & 0xFF);
            for (size_t b=0;b<4;b++) audioData.samples[off + 4 + b] = static_cast<uint8_t>((r >> (8*b)) & 0xFF);
        }

        bool ok = true;
        try {
            auto idx = AudioIndex::audioDataToIndex(audioData);
            auto audioData2 = AudioIndex::indexToAudioData(idx);
            ok &= RUN_CHECK(runner, name, audioData2.sample_rate == audioData.sample_rate, "sample_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.bit_rate == audioData.bit_rate, "bit_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.num_channels == audioData.num_channels, "num_channels match");
            ok &= RUN_CHECK(runner, name, audioData2.num_frames == audioData.num_frames, "num_frames match");
            ok &= RUN_CHECK(runner, name, audioData2.samples == audioData.samples, "samples content match");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
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
            auto safe_rm = [&](const std::string &p){ try{ std::filesystem::remove(p); } catch(...) {} };
            safe_rm(std::string("cpp/tests/indexes/") + prefix + ".txt");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    // ------------------ Additional unit tests ------------------
    runner.add("AudioIndex: 16-bit edge values roundtrip", [&runner]() -> bool {
        const std::string name = "AudioIndex: 16-bit edge values roundtrip";
        AudioIndex::AudioData audioData{};
        audioData.sample_rate = 44100;
        audioData.bit_rate = 16;
        audioData.num_channels = 1;
        audioData.audio_format = 1;
        audioData.num_frames = 2;
        // samples: INT16_MIN, INT16_MAX
        int16_t s0 = static_cast<int16_t>(std::numeric_limits<int16_t>::min());
        int16_t s1 = static_cast<int16_t>(std::numeric_limits<int16_t>::max());
        audioData.samples.resize(2 * 2);
        audioData.samples[0] = static_cast<uint8_t>(s0 & 0xFF);
        audioData.samples[1] = static_cast<uint8_t>((s0 >> 8) & 0xFF);
        audioData.samples[2] = static_cast<uint8_t>(s1 & 0xFF);
        audioData.samples[3] = static_cast<uint8_t>((s1 >> 8) & 0xFF);

        bool ok = true;
        try {
            auto idx = AudioIndex::audioDataToIndex(audioData);
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
        const std::string name = "AudioIndex: 32-bit edge values roundtrip";
        AudioIndex::AudioData audioData{};
        audioData.sample_rate = 48000;
        audioData.bit_rate = 32;
        audioData.num_channels = 1;
        audioData.audio_format = 1;
        audioData.num_frames = 2;
        int32_t s0 = std::numeric_limits<int32_t>::min();
        int32_t s1 = std::numeric_limits<int32_t>::max();
        audioData.samples.resize(2 * 4);
        for (size_t b = 0; b < 4; ++b) audioData.samples[b] = static_cast<uint8_t>((s0 >> (8*b)) & 0xFF);
        for (size_t b = 0; b < 4; ++b) audioData.samples[4 + b] = static_cast<uint8_t>((s1 >> (8*b)) & 0xFF);

        bool ok = true;
        try {
            auto idx = AudioIndex::audioDataToIndex(audioData);
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

    runner.add("AudioIndex: 6-channel 16-bit roundtrip", [&runner]() -> bool {
        const std::string name = "AudioIndex: 6-channel 16-bit roundtrip";
        AudioIndex::AudioData audioData{};
        audioData.sample_rate = 48000;
        audioData.bit_rate = 16;
        audioData.num_channels = 6;
        audioData.audio_format = 1;
        audioData.num_frames = 3; // 3 frames

        // Build simple interleaved pattern for 6 channels
        audioData.samples.resize(audioData.num_frames * audioData.num_channels * 2);
        for (size_t f = 0; f < audioData.num_frames; ++f) {
            for (uint16_t ch = 0; ch < audioData.num_channels; ++ch) {
                int16_t val = static_cast<int16_t>((int)f * 100 + (int)ch * 10 - 50);
                size_t off = (f * audioData.num_channels + ch) * 2;
                audioData.samples[off + 0] = static_cast<uint8_t>(val & 0xFF);
                audioData.samples[off + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
            }
        }

        bool ok = true;
        try {
            auto idx = AudioIndex::audioDataToIndex(audioData);
            auto audioData2 = AudioIndex::indexToAudioData(idx);
            ok &= RUN_CHECK(runner, name, audioData2.sample_rate == audioData.sample_rate, "sample_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.bit_rate == audioData.bit_rate, "bit_rate match");
            ok &= RUN_CHECK(runner, name, audioData2.num_channels == audioData.num_channels, "num_channels match");
            ok &= RUN_CHECK(runner, name, audioData2.num_frames == audioData.num_frames, "num_frames match");
            ok &= RUN_CHECK(runner, name, audioData2.samples == audioData.samples, "samples content match");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("AudioIndex: serialization textual roundtrip", [&runner]() -> bool {
        const std::string name = "AudioIndex: serialization textual roundtrip";
        using boost::multiprecision::cpp_int;
        AudioIndex::AudioData audioData = AudioIndex::extractAudioDataFromSamples(std::vector<int32_t>{0,12345,-12345}, 44100, 16);
        bool ok = true;
        try {
            cpp_int idx = AudioIndex::audioDataToIndex(audioData);
            std::string s = idx.convert_to<std::string>();
            cpp_int idx2(s);
            auto audioData2 = AudioIndex::indexToAudioData(idx2);
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
        audioData.sample_rate = 44100;
        audioData.bit_rate = 16;
        audioData.num_channels = 1;
        audioData.audio_format = 1;
        audioData.num_frames = 4;
        size_t bytes = audioData.num_frames * audioData.num_channels * (audioData.bit_rate/8);
        audioData.samples.resize(bytes);
        // samples: 1,2,3,4 -> little-endian bytes (LSB first), big-endian MSB will be zero
        for (size_t i = 0; i < audioData.num_frames; ++i) {
            int16_t v = static_cast<int16_t>(i + 1);
            size_t off = i * 2;
            audioData.samples[off + 0] = static_cast<uint8_t>(v & 0xFF);
            audioData.samples[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        }

        // Clear any previous debug info
        AudioIndex::clearLastDebugInfo();

        bool ok = true;
        try {
            auto idx = AudioIndex::audioDataToIndex(audioData);
            auto audioData2 = AudioIndex::indexToAudioData(idx);

            auto dbg = AudioIndex::getLastDebugInfo();
            size_t expected_bytes = audioData.num_frames * audioData.num_channels * (audioData.bit_rate/8);
            ok &= RUN_CHECK(runner, name, dbg.export_expected_bytes == expected_bytes, "export_expected_bytes equals expected");
            ok &= RUN_CHECK(runner, name, dbg.export_pcm_bytes == expected_bytes, "export_pcm_bytes was padded to expected");
            ok &= RUN_CHECK(runner, name, audioData2.samples == audioData.samples, "samples round-trip exactly");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    // ------------------ Integration: round-trip Test Audio files ------------------
    runner.add("AudioIndex: round-trip test audio directory", [&runner]() -> bool {
        const std::string name = "AudioIndex: round-trip test audio directory";
        namespace fs = std::filesystem;
        // Locate the Test Audio directory relative to the current working directory.
        auto locate_in_dir = [&]() -> fs::path {
            fs::path cur = fs::current_path();
            for (int i = 0; i < 6; ++i) {
                fs::path cand = cur / "cpp" / "tests" / "Test Audio";
                if (fs::exists(cand) && fs::is_directory(cand)) return cand;
                if (cur.has_parent_path()) cur = cur.parent_path(); else break;
            }
            return fs::path();
        };

        fs::path inDir = locate_in_dir();
        fs::path outDir = inDir / "Outputs";
        bool ok = true;
        try {
            if (!fs::exists(inDir) || !fs::is_directory(inDir)) {
                std::cout << "  [SKIP] " << name << " — tests/Test Audio directory not found\n";
                return true; // skip if test data not present
            }
            if (!fs::exists(outDir)) fs::create_directories(outDir);

            for (auto& ent : fs::directory_iterator(inDir)) {
                if (!ent.is_regular_file()) continue;
                auto p = ent.path();
                if (p.extension() != ".wav" && p.extension() != ".WAV") continue;
                std::vector<int32_t> samples;
                int sr = 0;

                // Log original properties
                std::ostringstream orig;
                orig << "FILE: " << p.string() << " | sr=" << sr << " | frames=" << samples.size() << " | bytes=" << (samples.size() * 2) ;
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
                    log_now(std::string("WROTE INDEX REPRS: cpp/tests/indexes/" ) + stem);
                } catch (const std::exception& e) {
                    log_now(std::string("WARN: failed to write index representations for: ") + p.string() + " err=" + e.what());
                } catch (...) {
                    log_now(std::string("WARN: failed to write index representations for: ") + p.string());
                }

                log_now("Reconstructing Audio Data from Index for: " + p.string());
                auto reconstructedData = AudioIndex::indexToAudioData(idx);

                // Log debug stats if available
                try {
                    auto dbg = AudioIndex::getLastDebugInfo();
                    std::ostringstream dbgss;
                    dbgss << "DEBUG: " << p.string() << " | import_bytes=" << dbg.import_pcm_bytes << " expected_import=" << dbg.import_expected_bytes
                        << " | export_bytes=" << dbg.export_pcm_bytes << " expected_export=" << dbg.export_expected_bytes
                        << " | ms_import=" << dbg.audioDataToIndexMs << " ms_export=" << dbg.indexToAudioDataMs;
                    log_now(dbgss.str());
                } catch (...) {}

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
                    for (size_t i = 0; i < originalData.samples.size(); ++i) if (originalData.samples[i] != reconstructedData.samples[i]) ++diffs;
                    std::ostringstream ss;
                    ss << "sample payload differs (" << diffs << " bytes) for: " << p.string();
                    runner.failMsg(name, ss.str());
                    log_now(std::string("FILE DIFF: ") + ss.str());
                    ok = false;
                }

                // Log reconstructed properties
                std::ostringstream recon;
                recon << "RECON: " << p.string() << " | sr=" << reconstructedData.sample_rate << " | frames=" << reconstructedData.num_frames << " | bytes=" << reconstructedData.samples.size();
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
    } catch(...) {}

    std::string filter;
    if (argc > 1) filter = argv[1];

    runner.runAll(filter);

    log_now(std::string("TEST RUN END: ") + std::to_string(runner.passed) + " passed, " + std::to_string(runner.failed) + " failed");
    if (g_log) g_log.close();
    return (runner.failed == 0) ? 0 : 1;
}
