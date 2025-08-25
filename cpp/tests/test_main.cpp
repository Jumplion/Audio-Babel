#include "AudioIndex.h"
#include "AudioFingerprint.h"
#include "AudioSearch.h"
#include "AudioBrowser.h"
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
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace AudioBabel;

// Lightweight test harness (no external framework)
struct TestRunner {
    int passed = 0;
    int failed = 0;
    std::map<std::string, std::function<bool()>> tests;

    void add(const std::string& name, const std::function<bool()>& fn) {
        tests[name] = fn;
    }

    static bool approxEqual(double a, double b, double tol = 1e-6) {
        return std::fabs(a - b) <= tol;
    }

    static bool vecNotEmpty(const std::vector<int32_t>& v) { return !v.empty(); }

    void failMsg(const std::string& test, const std::string& msg) {
        std::cout << "  ✗ " << test << " — " << msg << std::endl;
        ++failed;
    }

    void passMsg(const std::string& test) {
        std::cout << "  ✓ " << test << std::endl;
        ++passed;
    }

    bool runOne(const std::string& name) {
        auto it = tests.find(name);
        if (it == tests.end()) {
            std::cout << "Test not found: " << name << std::endl;
            return false;
        }
        // Spinner-based progress indicator
        std::atomic<bool> spinning{true};
        auto spinner_fn = [&]() {
            const char spinChars[] = {'|','/','-','\\'};
            int idx = 0;
            while (spinning.load()) {
                std::cout << '\r' << '[' << spinChars[idx % 4] << "] Running: " << name << std::flush;
                idx++;
                std::this_thread::sleep_for(std::chrono::milliseconds(120));
            }
            // clear the line after stopping
            std::cout << '\r' << std::string(80, ' ') << '\r' << std::flush;
        };

        std::thread spinner(spinner_fn);
        bool ok = false;
        auto t0 = std::chrono::steady_clock::now();
        try {
            ok = it->second();
        } catch (const std::exception& e) {
            spinning.store(false);
            spinner.join();
            failMsg(name, std::string("exception: ") + e.what());
            return false;
        } catch (...) {
            spinning.store(false);
            spinner.join();
            failMsg(name, "unknown exception");
            return false;
        }
        auto t1 = std::chrono::steady_clock::now();
        spinning.store(false);
        spinner.join();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        // Print elapsed time then pass/fail
        std::cout << "[" << ms << " ms] ";
        if (ok) passMsg(name); else failMsg(name, "assertions failed");
        return ok;
    }

    void runAll(const std::string& filter = "") {
        for (auto& kv : tests) {
            if (!filter.empty() && kv.first.find(filter) == std::string::npos) continue;
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

// Tests (AudioIndex only)
bool testAudioIndex_impl(TestRunner& runner) {
    const std::string name = "AudioIndex: basic";
    AudioIndex index1 = AudioIndex::fromHierarchy("genre1", "artist1", "album1", "track1");
    AudioIndex index2 = AudioIndex::fromHierarchy("genre1", "artist1", "album1", "track2");

    if (!CHECK(index1 == index1, runner, name, "self-equality")) return false;
    if (!CHECK(index1 != index2, runner, name, "inequality")) return false;
    if (!CHECK(index1.getGenreString() == "genre1", runner, name, "genre string")) return false;

    std::stringstream ss;
    index1.serialize(ss);
    ss.seekg(0);
    AudioIndex deserialized = AudioIndex::deserialize(ss);
    if (!CHECK(index1 == deserialized, runner, name, "serialize/deserialize")) return false;

    // Extra checks: fromAudioSamples -> duration and serialize round-trip
    {
        const int sr = 44100;
        std::vector<int32_t> samples(sr, 0); // 1 second of silence
        AudioIndex ai = AudioIndex::fromAudioSamples(samples, sr);
        if (!CHECK(TestRunner::approxEqual(ai.getDuration(), 1.0, 1e-6), runner, name, "duration from 1s samples")) return false;

        std::stringstream ss2;
        ai.serialize(ss2);
        ss2.seekg(0);
        AudioIndex ai2 = AudioIndex::deserialize(ss2);
        if (!CHECK(ai == ai2, runner, name, "serialize/deserialize fromAudioSamples")) return false;
    }

    // Edge case: very short audio should not crash and should report correct duration
    {
        const int sr = 44100;
        std::vector<int32_t> tiny(2, 12345);
        AudioIndex ai_short = AudioIndex::fromAudioSamples(tiny, sr);
        if (!CHECK(TestRunner::approxEqual(ai_short.getDuration(), 2.0 / sr, 1e-9), runner, name, "very short duration")) return false;

        std::stringstream ss3;
        ai_short.serialize(ss3);
        ss3.seekg(0);
        AudioIndex ai_short2 = AudioIndex::deserialize(ss3);
        if (!CHECK(ai_short == ai_short2, runner, name, "serialize/deserialize tiny")) return false;
    }
    return true;
}

// --- WAV loader helper (small, resilient, for tests only) ---
static bool loadWavToInt32(const std::string& path, std::vector<int32_t>& outSamples, int& outSampleRate) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    auto read_le = [&](void* buf, size_t n) { f.read(reinterpret_cast<char*>(buf), n); return f.gcount() == (std::streamsize)n; };

    // RIFF header
    char riff[4];
    if (!read_le(riff, 4)) return false;
    if (std::memcmp(riff, "RIFF", 4) != 0) return false;
    uint32_t riff_size;
    if (!read_le(&riff_size, 4)) return false;
    char wave[4];
    if (!read_le(wave,4)) return false;
    if (std::memcmp(wave, "WAVE", 4) != 0) return false;

    bool gotFmt = false, gotData = false;
    uint16_t audioFormat = 0, numChannels = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0, byteRate = 0;
    std::vector<char> dataChunk;

    while (!gotFmt || !gotData) {
        char chunkId[4];
        if (!read_le(chunkId, 4)) break;
        uint32_t chunkSize = 0;
        if (!read_le(&chunkSize, 4)) break;
        std::streampos nextPos = f.tellg();
        nextPos += static_cast<std::streamoff>(chunkSize + (chunkSize & 1)); // pad to even

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            // fmt chunk
            if (chunkSize < 16) return false;
            if (!read_le(&audioFormat, 2)) return false;
            if (!read_le(&numChannels, 2)) return false;
            if (!read_le(&sampleRate, 4)) return false;
            if (!read_le(&byteRate, 4)) return false;
            uint16_t blockAlign = 0;
            if (!read_le(&blockAlign, 2)) return false;
            if (!read_le(&bitsPerSample, 2)) return false;
            // skip any extra fmt bytes
            if (chunkSize > 16) {
                f.seekg(chunkSize - 16, std::ios::cur);
            }
            gotFmt = true;
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            dataChunk.resize(chunkSize);
            if (!read_le(dataChunk.data(), chunkSize)) return false;
            gotData = true;
        } else {
            // skip unknown chunk
            f.seekg(chunkSize, std::ios::cur);
        }
        // seek to chunk boundary (handle odd padding)
        if (f.tellg() != nextPos) f.seekg(nextPos);
    }

    if (!gotFmt || !gotData) return false;
    if (sampleRate == 0) return false;

    outSampleRate = static_cast<int>(sampleRate);
    outSamples.clear();

    const size_t frameCount = dataChunk.size() / (numChannels * (bitsPerSample/8));

    if (audioFormat == 1) {
        // PCM integer
        if (bitsPerSample == 16) {
            const int16_t* src = reinterpret_cast<const int16_t*>(dataChunk.data());
            for (size_t i = 0; i < frameCount; ++i) {
                int64_t acc = 0;
                for (uint16_t ch = 0; ch < numChannels; ++ch) acc += src[i * numChannels + ch];
                int16_t avg = static_cast<int16_t>(acc / numChannels);
                outSamples.push_back(static_cast<int32_t>(avg) << 16); // scale to 32-bit range
            }
        } else if (bitsPerSample == 32) {
            const int32_t* src = reinterpret_cast<const int32_t*>(dataChunk.data());
            for (size_t i = 0; i < frameCount; ++i) {
                int64_t acc = 0;
                for (uint16_t ch = 0; ch < numChannels; ++ch) acc += src[i * numChannels + ch];
                int32_t avg = static_cast<int32_t>(acc / numChannels);
                outSamples.push_back(avg);
            }
        } else {
            return false; // unsupported PCM bit depth for tests
        }
    } else if (audioFormat == 3) {
        // IEEE float
        const float* src = reinterpret_cast<const float*>(dataChunk.data());
        for (size_t i = 0; i < frameCount; ++i) {
            double acc = 0.0;
            for (uint16_t ch = 0; ch < numChannels; ++ch) acc += src[i * numChannels + ch];
            float avg = static_cast<float>(acc / numChannels);
            int32_t sample = static_cast<int32_t>(std::max(-1.0f, std::min(1.0f, avg)) * static_cast<float>(INT32_MAX));
            outSamples.push_back(sample);
        }
    } else {
        return false; // unsupported format
    }

    return true;
}

bool testAudioIndex_wav_impl(TestRunner& runner) {
    const std::string name = "AudioIndex: wav files";
    // Clear previous log
    try {
        std::ofstream clearLog("tests/test_results.log", std::ios::trunc);
        if (clearLog) clearLog.close();
    } catch (...) {}

    // Enumerate WAV files under tests/Test Audio
    namespace fs = std::filesystem;
    std::vector<std::string> files;
    try {
        for (auto &entry : fs::directory_iterator("tests/Test Audio")) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
            if (ext == ".wav") files.push_back(entry.path().string());
        }
        std::sort(files.begin(), files.end());
    } catch (...) {
        // ignore directory errors; will result in empty files vector
    }

    for (const auto& rel : files) {
        std::vector<int32_t> samples;
        int sr = 0;
        if (!loadWavToInt32(rel, samples, sr)) {
            runner.failMsg(name, std::string("failed to load: ") + rel);
            return false;
        }
        if (!CHECK(!samples.empty(), runner, name, rel + " non-empty samples")) return false;
        double duration = static_cast<double>(samples.size()) / sr;
        if (!CHECK(duration > 0.0, runner, name, rel + " duration>0")) return false;

        AudioIndex ai = AudioIndex::fromAudioSamples(samples, sr);
        if (!CHECK(TestRunner::approxEqual(ai.getDuration(), duration, 1e-3), runner, name, rel + " duration match")) return false;

        std::stringstream ss;
        ai.serialize(ss);
        ss.seekg(0);
        AudioIndex ai2 = AudioIndex::deserialize(ss);
        if (!CHECK(ai == ai2, runner, name, rel + " roundtrip")) return false;

        // --- Logging: write index hex blobs and basic metadata to a log file ---
        try {
            const std::string logPath = "tests/test_results.log"; // relative to cpp/ working dir
            std::ofstream log(logPath, std::ios::app);
            if (log) {
                // Re-serialize to parse the raw mpz blobs
                std::stringstream s2;
                ai.serialize(s2);
                s2.seekg(0);
                // Skip fixed-size header written by AudioIndex::serialize: int(4) + double(8) + int(4) = 16 bytes
                s2.seekg(16);

                auto read_u64_le = [&](uint64_t &v)->bool {
                    uint64_t x=0;
                    char buf[8];
                    if (!s2.read(buf,8)) return false;
                    for (int i=0;i<8;++i) x |= (static_cast<uint64_t>(static_cast<unsigned char>(buf[i])) << (8*i));
                    v = x; return true;
                };

                uint64_t len;
                std::vector<std::string> blobs;
                for (int i = 0; i < 4; ++i) {
                    if (!read_u64_le(len)) break;
                    std::vector<unsigned char> buf(len);
                    if (!s2.read(reinterpret_cast<char*>(buf.data()), len)) break;
                    // hex dump
                    std::ostringstream h;
                    h << std::hex << std::setfill('0');
                    for (auto b : buf) h << std::setw(2) << static_cast<int>(b);
                    blobs.push_back(h.str());
                }

                // Fingerprint length from the object (reliable)
                uint64_t fpLen = static_cast<uint64_t>(ai.getFingerprint().size());

                log << "File=" << rel << " SR=" << sr << " Dur=" << duration << " FPbytes=" << fpLen << " IndexPath=" << ai.getFullPath() << "\n";
                for (size_t i = 0; i < blobs.size(); ++i) {
                    log << "  part" << i << "=" << blobs[i] << "\n";
                }
                log << "---\n";
                log.flush();
                log.close();
            }
        } catch (...) {
            // non-fatal: logging failure should not break tests
            std::cout << "  ! Warning: failed to write log for " << rel << std::endl;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    TestRunner runner;
    runner.add("AudioIndex: basic", std::bind(testAudioIndex_impl, std::ref(runner)));
    runner.add("AudioIndex: wav files", std::bind(testAudioIndex_wav_impl, std::ref(runner)));

    std::string filter;
    if (argc > 1) filter = argv[1];

    runner.runAll(filter);
    return (runner.failed == 0) ? 0 : 1;
}
