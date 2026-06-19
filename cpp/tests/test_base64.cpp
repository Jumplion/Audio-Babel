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

