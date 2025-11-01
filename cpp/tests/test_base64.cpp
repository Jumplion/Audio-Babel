/**
 * @file test_base64.cpp
 * @brief Unit tests for Base64 URL-safe encoding/decoding.
 * 
 * Tests the Utilities::encodeBase64Url and Utilities::decodeBase64Url functions
 * for correctness, edge cases, and validation behavior.
 * 
 * Migrated to Catch2 v3 framework.
 */

#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace AudioBabel::Utilities;

TEST_CASE("Base64Url: alphabet and edge-case roundtrip", "[base64][edge_case]") {
    SECTION("Alphabet constant should be 64 chars") {
        REQUIRE(std::char_traits<char>::length(BASE64_URL_ALPHA) == 64);
    }

    SECTION("Empty input roundtrip") {
        std::vector<uint8_t> in0;
        std::string          s0   = encodeBase64Url(in0);
        auto                 out0 = decodeBase64Url(s0);
        REQUIRE(out0.empty());
    }

    SECTION("All Single byte values roundtrip") {
        for (int x = 0; x <= 255; ++x) {
            std::vector<uint8_t> in  = {static_cast<uint8_t>(x)};
            std::string          s   = encodeBase64Url(in);
            auto                 out = decodeBase64Url(s);

            REQUIRE(out == in);
        }
    }

    SECTION("Random Multi-byte sequence") {
        // Random multi-byte sequence
        for (int i = 0; i < 100; ++i) {
            // Random length between 4 and 13 bytes
            int length = 4 + (rand() % 10);

            // Randomly generate a multi-byte vector
            std::vector<uint8_t> in;
            in.reserve(length);
            for (int j = 0; j < length; ++j) {
                in.push_back(static_cast<uint8_t>(rand() % 256));
            }

            INFO("Testing byte sequence: " + std::to_string(i));
            std::string s   = encodeBase64Url(in);
            auto        out = decodeBase64Url(s);
            REQUIRE(out == in);
        }
    }
}

TEST_CASE("Base64Url: validation behavior", "[base64][validation]") {
    SECTION("Valid URL-safe base64 strings (no padding)") {
        REQUIRE(isValidBase64Url(""));
        REQUIRE(isValidBase64Url("A"));
        REQUIRE(isValidBase64Url("Ab0-_"));
    }

    SECTION("Invalid characters should be rejected") {
        REQUIRE_FALSE(isValidBase64Url("A="));   // Padding '=' invalid
        REQUIRE_FALSE(isValidBase64Url("!"));    // '!' invalid
        REQUIRE_FALSE(isValidBase64Url("A B"));  // Space invalid
        REQUIRE_FALSE(isValidBase64Url("A\tB")); // Tab invalid
        REQUIRE_FALSE(isValidBase64Url("A\nB")); // Newline invalid
        REQUIRE_FALSE(isValidBase64Url("A\rB")); // Carriage return invalid

        // Test invalid high-bit character (construct string to avoid literal warnings)
        std::string invalid_byte = "A";
        invalid_byte += static_cast<char>(0x80);
        invalid_byte += "B";
        REQUIRE_FALSE(isValidBase64Url(invalid_byte));

        REQUIRE_FALSE(isValidBase64Url("A=B"));  // '=' in middle invalid
        REQUIRE_FALSE(isValidBase64Url("A/B"));  // '/' invalid
        REQUIRE_FALSE(isValidBase64Url("A+B"));  // '+' invalid
        REQUIRE_FALSE(isValidBase64Url("A@B"));  // '@' invalid
        REQUIRE_FALSE(isValidBase64Url("A,B"));  // ',' invalid
        REQUIRE_FALSE(isValidBase64Url("A;B"));  // ';' invalid
        REQUIRE_FALSE(isValidBase64Url("A:B"));  // ':' invalid
        REQUIRE_FALSE(isValidBase64Url("A[B]")); // '[' invalid
        REQUIRE_FALSE(isValidBase64Url("A]B"));  // ']' invalid
        REQUIRE_FALSE(isValidBase64Url("A{B}")); // '{' invalid
        REQUIRE_FALSE(isValidBase64Url("A}B"));  // '}' invalid
        REQUIRE_FALSE(isValidBase64Url("A|B"));  // '|' invalid
        REQUIRE_FALSE(isValidBase64Url("A\\B")); // '\' invalid
        REQUIRE_FALSE(isValidBase64Url("A\"B")); // '\"' invalid
        REQUIRE_FALSE(isValidBase64Url("A'B"));  // '\'' invalid
        REQUIRE_FALSE(isValidBase64Url("A<B>")); // '<' invalid
        REQUIRE_FALSE(isValidBase64Url("A>B"));  // '>' invalid
        REQUIRE_FALSE(isValidBase64Url("A?B"));  // '?' invalid
    }

    SECTION("extractMetadataFromIndex throws on invalid base64") {
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A=")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("!")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A B")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A\tB")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A\nB")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A\rB")));

        // Test invalid high-bit character
        std::string invalid_byte_str = "A";
        invalid_byte_str += static_cast<char>(0x80);
        invalid_byte_str += "B";
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(invalid_byte_str));

        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A/B")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A+B")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A@B")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A,B")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A;B")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A:B")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A[B]")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A]B")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A{B}")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A}B")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A|B")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A\\B")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A\"B")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A'B")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A<B>")));
        REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(std::string("A>B")));
    }
}

TEST_CASE("Base64Url: full byte range roundtrip (0x00-0xFF)", "[base64][comprehensive]") {
    // Create a vector containing all 256 possible byte values
    std::vector<uint8_t> all_bytes;
    all_bytes.reserve(256);
    for (int i = 0; i < 256; ++i) {
        all_bytes.push_back(static_cast<uint8_t>(i));
    }

    // Encode to Base64
    std::string encoded = encodeBase64Url(all_bytes);

    SECTION("Encoded string contains only valid Base64 URL-safe characters") {
        for (char c : encoded) {
            bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
            REQUIRE(valid);
        }
    }

    // Decode back to bytes
    auto decoded = decodeBase64Url(encoded);

    SECTION("Decoded size matches original") {
        REQUIRE(decoded.size() == all_bytes.size());
    }

    SECTION("All 256 bytes match after roundtrip") {
        REQUIRE(decoded.size() == all_bytes.size());
        for (size_t i = 0; i < all_bytes.size(); ++i) {
            INFO("Byte index: " << i);
            REQUIRE(decoded[i] == all_bytes[i]);
        }
    }
}

TEST_CASE("Base64Url: 0-1024 length inputs roundtrip", "[base64][comprehensive]") {
    // Test various input lengths to verify padding logic
    for (size_t x = 0; x <= 1024; ++x) {
        INFO("Testing length: " << x);

        std::vector<uint8_t> input;
        input.reserve(x);
        for (size_t i = 0; i < x; ++i) {
            // Use a pattern that includes variety
            input.push_back(static_cast<uint8_t>((i * 7 + 13) % 256));
        }

        std::string encoded = encodeBase64Url(input);
        auto        decoded = decodeBase64Url(encoded);

        REQUIRE(decoded == input);
    }
}

TEST_CASE("Base64Url: boundary byte patterns", "[base64][edge_case]") {
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
        INFO("Testing pattern starting with 0x" << std::hex << static_cast<int>(pattern[0]));

        std::string encoded = encodeBase64Url(pattern);
        auto        decoded = decodeBase64Url(encoded);

        REQUIRE(decoded == pattern);
    }
}
