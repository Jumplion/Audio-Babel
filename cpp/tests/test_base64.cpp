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

    runner.add("Base64Url: full byte range roundtrip (0x00-0xFF)", [&runner]() -> bool {
        const std::string name = "Base64Url: full byte range roundtrip (0x00-0xFF)";
        using AudioBabel::Utilities::encodeBase64Url;
        using AudioBabel::Utilities::decodeBase64Url;
        bool ok = true;

        try {
            // Create a vector containing all 256 possible byte values
            std::vector<uint8_t> all_bytes;
            all_bytes.reserve(256);
            for (int i = 0; i < 256; ++i) {
                all_bytes.push_back(static_cast<uint8_t>(i));
            }

            // Encode to Base64
            std::string encoded = encodeBase64Url(all_bytes);

            // Verify encoded string contains only valid Base64 URL-safe characters
            for (char c : encoded) {
                bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
                if (!valid) {
                    runner.failMsg(name, std::string("Invalid character in encoded string: ") + c);
                    ok = false;
                    break;
                }
            }
            ok &= RUN_CHECK(runner, name, ok, "encoded string contains only valid Base64 URL-safe chars");

            // Decode back to bytes
            auto decoded = decodeBase64Url(encoded);

            // Verify the decoded bytes match the original
            ok &= RUN_CHECK(runner, name, decoded.size() == all_bytes.size(), "decoded size matches original (256 bytes)");

            if (decoded.size() == all_bytes.size()) {
                bool content_match = true;
                for (size_t i = 0; i < all_bytes.size(); ++i) {
                    if (decoded[i] != all_bytes[i]) {
                        std::ostringstream oss;
                        oss << "byte mismatch at index " << i << ": expected 0x" << std::hex << static_cast<int>(all_bytes[i]) << ", got 0x"
                            << static_cast<int>(decoded[i]);
                        runner.failMsg(name, oss.str());
                        content_match = false;
                        break;
                    }
                }
                ok &= RUN_CHECK(runner, name, content_match, "all 256 bytes match after roundtrip");
            }

            return ok;
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            return false;
        }
    });

    runner.add("Base64Url: various length inputs roundtrip", [&runner]() -> bool {
        const std::string name = "Base64Url: various length inputs roundtrip";
        using AudioBabel::Utilities::encodeBase64Url;
        using AudioBabel::Utilities::decodeBase64Url;
        bool ok = true;

        try {
            // Test various input lengths to verify padding logic
            // Base64 encoding works in 3-byte chunks -> 4-char output
            // Lengths that aren't multiples of 3 require special handling
            std::vector<size_t> test_lengths = {
                0,
                1,
                2,
                3, // Edge cases: empty and small
                4,
                5,
                6, // Multiple of 3 and near multiples
                15,
                16,
                17, // Larger multiples
                63,
                64,
                65, // Around power-of-2 boundary
                100,
                255,
                256, // Larger values
                1000,
                1024 // Even larger values
            };

            for (size_t len : test_lengths) {
                std::vector<uint8_t> input;
                input.reserve(len);
                for (size_t i = 0; i < len; ++i) {
                    // Use a pattern that includes variety
                    input.push_back(static_cast<uint8_t>((i * 7 + 13) % 256));
                }

                std::string encoded = encodeBase64Url(input);
                auto        decoded = decodeBase64Url(encoded);

                if (decoded != input) {
                    std::ostringstream oss;
                    oss << "roundtrip failed for length " << len;
                    runner.failMsg(name, oss.str());
                    ok = false;
                    break;
                }
            }

            ok &= RUN_CHECK(runner, name, ok, "all length variants roundtrip successfully");
            return ok;
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            return false;
        }
    });

    runner.add("Base64Url: boundary byte patterns", [&runner]() -> bool {
        const std::string name = "Base64Url: boundary byte patterns";
        using AudioBabel::Utilities::encodeBase64Url;
        using AudioBabel::Utilities::decodeBase64Url;
        bool ok = true;

        try {
            // Test specific patterns that might expose edge cases
            std::vector<std::vector<uint8_t>> test_patterns = {
                {0x00, 0x00, 0x00},                              // All zeros
                {0xFF, 0xFF, 0xFF},                              // All ones
                {0x00, 0xFF, 0x00},                              // Alternating
                {0xAA, 0xAA, 0xAA},                              // 10101010 pattern
                {0x55, 0x55, 0x55},                              // 01010101 pattern
                {0x00, 0x00, 0x01},                              // Minimal value
                {0xFF, 0xFF, 0xFE},                              // Maximal value minus 1
                {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0} // Sequential pattern
            };

            for (const auto& pattern : test_patterns) {
                std::string encoded = encodeBase64Url(pattern);
                auto        decoded = decodeBase64Url(encoded);

                if (decoded != pattern) {
                    std::ostringstream oss;
                    oss << "roundtrip failed for pattern starting with 0x" << std::hex << static_cast<int>(pattern[0]);
                    runner.failMsg(name, oss.str());
                    ok = false;
                    break;
                }
            }

            ok &= RUN_CHECK(runner, name, ok, "all boundary patterns roundtrip successfully");
            return ok;
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            return false;
        }
    });
}
