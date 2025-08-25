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
        std::cout << "Running: " << name << std::endl;
        bool ok = false;
        try {
            ok = it->second();
        } catch (const std::exception& e) {
            failMsg(name, std::string("exception: ") + e.what());
            return false;
        } catch (...) {
            failMsg(name, "unknown exception");
            return false;
        }
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

int main(int argc, char** argv) {
    TestRunner runner;
    runner.add("AudioIndex: basic", std::bind(testAudioIndex_impl, std::ref(runner)));

    std::string filter;
    if (argc > 1) filter = argv[1];

    runner.runAll(filter);
    return (runner.failed == 0) ? 0 : 1;
}
