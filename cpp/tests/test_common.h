/**
 * @file test_common.h
 * @brief Shared test infrastructure for the Audio Babel test suite.
 * 
 * This header provides a lightweight, framework-free test harness and utilities
 * used across all test modules. It intentionally avoids external dependencies
 * so it can be built with the project's normal toolchain.
 * 
 * @section test_infrastructure Test Infrastructure
 * - TestRunner: Test harness that tracks pass/fail counts
 * - CHECK/RUN_CHECK: Assertion helpers
 * - log_now: Timestamped logging
 * - make_temp_path: Temporary file path generation
 * - TempFile: RAII temporary file management
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

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
#include "IndexMetadata.h"
#include "LibraryPosition.h"
#include "Utilities.h"

#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

using namespace AudioBabel;

// Global log file used by the test runner and integration tests
extern std::ofstream g_log;

/**
 * @brief Log a timestamped message to the global log file.
 * @param msg Message to log
 * @param printToConsole If true, also print to stdout
 */
inline void log_now(const std::string& msg, bool printToConsole = false) {
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
 * @struct TestRunner
 * @brief Lightweight test harness that tracks test execution and results.
 * 
 * TestRunner provides a simple framework-free test infrastructure for registering
 * and running tests. Each test is a callable returning bool (true = pass, false = fail).
 * 
 * @par Usage Example
 * @code
 * TestRunner runner;
 * runner.add("My Test", [&runner]() -> bool {
 *     CHECK(1 + 1 == 2, runner, "My Test", "basic math");
 *     return true;
 * });
 * runner.runAll();
 * @endcode
 */
struct TestRunner {
    int                                          passed = 0; ///< Number of tests passed
    int                                          failed = 0; ///< Number of tests failed
    std::map<std::string, std::function<bool()>> tests;      ///< Registered tests

    /**
     * @brief Register a test with the runner.
     * @param name Human-readable test name shown in console output
     * @param fn Callable returning true on success, false on failure
     */
    void add(const std::string& name, const std::function<bool()>& fn) {
        tests[name] = fn;
    }

    /**
     * @brief Check if two floating-point values are approximately equal.
     * @param a First value
     * @param b Second value
     * @param tol Tolerance (default: 1e-6)
     * @return true if |a - b| <= tol
     */
    static auto approxEqual(double a, double b, double tol = 1e-6) -> bool {
        return std::fabs(a - b) <= tol;
    }

    /**
     * @brief Check if a vector is not empty.
     * @param v Vector to check
     * @return true if vector has at least one element
     */
    static auto vecNotEmpty(const std::vector<int32_t>& v) -> bool {
        return !v.empty();
    }

    /**
     * @brief Record a test failure with a descriptive message.
     * @param test Test name
     * @param msg Failure message
     * 
     * @note Increments the failure counter and prints a failure line.
     */
    void failMsg(const std::string& test, const std::string& msg) {
        std::cout << "  ✗ " << test << " — " << msg << '\n';
        ++failed;
    }

    /**
     * @brief Record a test success.
     * @param test Test name
     * 
     * @note Increments the pass counter and prints a success line.
     */
    void passMsg(const std::string& test) {
        std::cout << "  ✓ " << test << '\n';
        ++passed;
    }

    /**
     * @brief Run a single test by name.
     * @param name Name of the test to run
     * @return true if test passed, false otherwise
     * 
     * @note Measures test duration and logs results. Catches and reports exceptions.
     */
    auto runOne(const std::string& name) -> bool {
        auto it = tests.find(name);
        if (it == tests.end()) {
            std::cout << "Test not found: " << name << '\n';
            return false;
        }

        // Capture failure count before/after to avoid double-counting assertion failures
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

        // If the test passed, increment pass counter. If it failed but already
        // produced failure messages, don't double-count.
        if (ok) {
            ++passed;
            log_now(std::string("PASS: ") + name + " (" + std::to_string(ms) + "ms)");
        } else {
            log_now(std::string("FAIL: ") + name + " (" + std::to_string(ms) + "ms)");
            if (!exceptionMsg.empty()) {
                log_now("  " + exceptionMsg);
                std::cout << "  Exception: " << exceptionMsg << '\n';
            }
            if (failed == static_cast<int>(failed_before)) {
                // No assertion failures reported; count this as one failure
                ++failed;
            }
        }
        return ok;
    }

    /**
     * @brief Run all registered tests, optionally filtered by name substring.
     * @param filter If non-empty, only run tests whose names contain this substring
     * 
     * @note Prints a summary of passed/failed tests at the end.
     */
    void runAll(const std::string& filter = "") {
        for (auto& kv : tests) {
            if (!filter.empty() && kv.first.find(filter) == std::string::npos) {
                continue;
            }
            log_now(std::string("============== RUNNING TEST: [") + kv.first + "] ==============");
            std::cout << "============== RUNNING TEST: [" << kv.first << "] ==============" << '\n';
            runOne(kv.first);
        }
        std::cout << "\nSummary: " << passed << " passed, " << failed << " failed" << '\n';
    }
};

/**
 * @brief Basic assertion helper used inside tests (OLD FRAMEWORK - being phased out).
 * @param cond Condition to check
 * @param runner TestRunner instance (for recording failures)
 * @param test Test name
 * @param msg Failure message (optional)
 * @return true if condition passed, false otherwise
 * 
 * @note Does not throw; instead records failure in the runner.
 * @deprecated Use Catch2's REQUIRE/CHECK macros instead
 */
inline auto OLD_CHECK(bool cond, TestRunner& runner, const std::string& test, const std::string& msg = "") -> bool {
    if (!cond) {
        runner.failMsg(test, msg.empty() ? "check failed" : msg);
        return false;
    }
    return true;
}

/**
 * @brief Assertion helper that prints per-assertion status (OLD FRAMEWORK - being phased out).
 * @param runner TestRunner instance
 * @param testName Name of the test
 * @param cond Condition to check
 * @param msg Descriptive message for the assertion
 * @return true if condition passed, false otherwise
 * 
 * @note Prints [OK] or [FAIL] prefix for each assertion.
 * @deprecated Use Catch2's REQUIRE/CHECK macros instead
 */
inline auto RUN_CHECK(TestRunner& runner, const std::string& testName, bool cond, const std::string& msg) -> bool {
    bool ok = OLD_CHECK(cond, runner, testName, msg);
    if (ok) {
        // std::cout << "  [OK]   " << msg << '\n';
    } else {
        std::cout << "  [FAIL] " << msg << '\n';
    }
    return ok;
}

#endif // TEST_COMMON_H
