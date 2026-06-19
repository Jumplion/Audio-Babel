/**
 * @file test_base64.cpp
 * @brief Unit tests for Base64 URL-safe character validation.
 *
 * Tests the Utilities::isValidBase64Url function and the validation it
 * performs at the IndexMetadata::extractMetadataFromIndex boundary.
 *
 * Migrated to Catch2 v3 framework.
 */

#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace AudioBabel::Utilities;

TEST_CASE("Base64Url: alphabet constant", "[base64][edge_case]") {
    REQUIRE(std::char_traits<char>::length(BASE64_URL_ALPHA) == 64);
}

TEST_CASE("Base64Url: validation behavior", "[base64][validation]") {
    SECTION("Valid URL-safe base64 strings (no padding)") {
        REQUIRE(isValidBase64Url(""));
        REQUIRE(isValidBase64Url("A"));
        REQUIRE(isValidBase64Url("Ab0-_"));
    }

    SECTION("Invalid characters are rejected by both isValidBase64Url and extractMetadataFromIndex") {
        // Construct the high-bit-byte and mid-string '=' cases (avoid raw literal warnings).
        std::string invalid_byte = "A";
        invalid_byte += static_cast<char>(0x80);
        invalid_byte += "B";

        std::vector<std::string> invalid = {
            "A=",   // Padding '=' invalid
            "!",    // '!' invalid
            "A B",  // Space invalid
            "A\tB", // Tab invalid
            "A\nB", // Newline invalid
            "A\rB", // Carriage return invalid
            invalid_byte,
            "A=B",  // '=' in middle invalid
            "A/B",  // '/' invalid
            "A+B",  // '+' invalid
            "A@B",  // '@' invalid
            "A,B",  // ',' invalid
            "A;B",  // ';' invalid
            "A:B",  // ':' invalid
            "A[B]", // '[' invalid
            "A]B",  // ']' invalid
            "A{B}", // '{' invalid
            "A}B",  // '}' invalid
            "A|B",  // '|' invalid
            "A\\B", // '\' invalid
            "A\"B", // '\"' invalid
            "A'B",  // '\'' invalid
            "A<B>", // '<' invalid
            "A>B",  // '>' invalid
            "A?B",  // '?' invalid
        };

        for (const auto& s : invalid) {
            INFO("Invalid input: '" << s << "'");
            REQUIRE_FALSE(isValidBase64Url(s));
            REQUIRE_THROWS(IndexMetadata::extractMetadataFromIndex(s));
        }
    }
}

