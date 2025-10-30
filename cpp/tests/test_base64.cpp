/**
 * @file test_base64.cpp
 * @brief Unit tests for Base64 URL-safe encoding/decoding.
 * 
 * Tests the Utilities::encodeBase64Url and Utilities::decodeBase64Url functions
 * for correctness, edge cases, and validation behavior.
 */

#include "test_common.h"

/**
 * @brief Register all Base64-related tests with the test runner.
 * @param runner TestRunner instance to register tests with
 */
void register_base64_tests(TestRunner& runner) {
    runner.add("Base64Url: encode/decode roundtrip", [&runner]() -> bool {
        const std::string name = "Base64Url: encode/decode roundtrip";
        using AudioBabel::Utilities::encodeBase64Url;
        using AudioBabel::Utilities::decodeBase64Url;
        std::vector<uint8_t> in = {0x00, 0x12, 0x34, 0xFF, 0x80};
        try {
            std::string s   = encodeBase64Url(in);
            auto        out = decodeBase64Url(s);
            bool        ok  = RUN_CHECK(runner, name, out == in, "roundtrip equality");
            return ok;
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            return false;
        }
    });

    runner.add("Base64Url: alphabet and edge-case roundtrip", [&runner]() -> bool {
        const std::string name = "Base64Url: alphabet and edge-case roundtrip";
        bool              ok   = true;
        using AudioBabel::Utilities::encodeBase64Url;
        using AudioBabel::Utilities::decodeBase64Url;

        // Alphabet constant should be 64 chars
        ok &= RUN_CHECK(runner, name, std::char_traits<char>::length(AudioBabel::Utilities::BASE64_URL_ALPHA) == 64, "alphabet length == 64");

        // Empty input roundtrip
        std::vector<uint8_t> in0;
        std::string          s0   = encodeBase64Url(in0);
        auto                 out0 = decodeBase64Url(s0);
        ok &= RUN_CHECK(runner, name, out0.empty(), "empty roundtrip");

        // Single byte roundtrip
        std::vector<uint8_t> in1  = {0xFF};
        std::string          s1   = encodeBase64Url(in1);
        auto                 out1 = decodeBase64Url(s1);
        ok &= RUN_CHECK(runner, name, out1 == in1, "single-byte roundtrip");

        return ok;
    });

    runner.add("IndexMetadata: isValidBase64 and decode behavior", [&runner]() -> bool {
        const std::string name = "IndexMetadata: isValidBase64 and decode behavior";
        bool              ok   = true;
        // Valid URL-safe base64 strings (no padding)
        ok &= RUN_CHECK(runner, name, AudioBabel::Utilities::isValidBase64Url(""), "empty string valid");
        ok &= RUN_CHECK(runner, name, AudioBabel::Utilities::isValidBase64Url("A"), "single A valid");
        ok &= RUN_CHECK(runner, name, AudioBabel::Utilities::isValidBase64Url("Ab0-_"), "chars allowed");

        // Invalid characters: '=' padding and '!' should be rejected by isValidBase64
        ok &= RUN_CHECK(runner, name, !AudioBabel::Utilities::isValidBase64Url("A="), "padding '=' invalid");
        ok &= RUN_CHECK(runner, name, !AudioBabel::Utilities::isValidBase64Url("!"), "'!' invalid");

        // extractMetadataFromIndex should throw for invalid base64 input
        bool threw = false;
        try {
            IndexMetadata m = IndexMetadata::extractMetadataFromIndex(std::string("A="));
        } catch (const std::exception&) {
            threw = true;
        }
        ok &= RUN_CHECK(runner, name, threw, "extractMetadataFromIndex throws on invalid base64");
        return ok;
    });
}
