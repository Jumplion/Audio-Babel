/**
 * @file test_metadata.cpp
 * @brief Unit tests for IndexMetadata extraction and generation.
 * 
 * Tests the IndexMetadata class including:
 * - Metadata extraction from big integer indexes
 * - Metadata extraction from base64 strings
 * - SVG cover generation
 * - Field validation and determinism
 * - Malformed input handling
 * 
 * Migrated to Catch2 v3 framework.
 */

#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace AudioBabel;
using boost::multiprecision::cpp_int;

// Helper to encode bytes to base64 URL-safe (no padding). Reuses the real
// alphabet (Utilities::BASE64_URL_ALPHA) rather than a hand-copied literal,
// so the two never drift apart; valid_b64_chars (test_common.h) still
// independently re-derives the *character set* to cross-check the result.
static std::string encode_b64_url(const std::vector<uint8_t>& bytes) {
    using AudioBabel::Utilities::BASE64_URL_ALPHA;
    std::string b64str;
    b64str.reserve((bytes.size() * 8 + 5) / 6);
    uint32_t acc      = 0;
    int      acc_bits = 0;
    for (uint8_t byte : bytes) {
        acc = (acc << 8) | byte;
        acc_bits += 8;
        while (acc_bits >= 6) {
            acc_bits -= 6;
            auto idx = static_cast<uint8_t>((acc >> acc_bits) & 0x3F);
            b64str.push_back(BASE64_URL_ALPHA[idx]);
        }
    }
    if (acc_bits > 0) {
        auto idx = static_cast<uint8_t>((acc << (6 - acc_bits)) & 0x3F);
        b64str.push_back(BASE64_URL_ALPHA[idx]);
    }
    return b64str;
}

TEST_CASE("IndexMetadata: deterministic and valid across both extraction overloads", "[metadata][determinism][base64]") {
    SECTION("cpp_int overload") {
        // Build a sample byte vector (non-empty) and construct a cpp_int (MSB-first)
        std::vector<uint8_t> bytes = {0x10, 0x20, 0x30, 0x41, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA,
                                      0xBB, 0xCC, 0xDD, 0xEE, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
        cpp_int              idx   = 0;
        for (uint8_t b : bytes) {
            idx <<= 8;
            idx |= cpp_int(static_cast<uint32_t>(b));
        }

        auto m1 = IndexMetadata::extractMetadataFromIndex(idx);
        auto m2 = IndexMetadata::extractMetadataFromIndex(idx);

        REQUIRE(m1.genre == m2.genre);
        REQUIRE(m1.artist == m2.artist);
        REQUIRE(m1.album == m2.album);
        REQUIRE(m1.track == m2.track);

        REQUIRE_FALSE(m1.genre.empty());
        REQUIRE_FALSE(m1.artist.empty());
        REQUIRE_FALSE(m1.album.empty());
        REQUIRE_FALSE(m1.track.empty());

        REQUIRE(valid_b64_chars(m1.genre));
        REQUIRE(valid_b64_chars(m1.artist));
        REQUIRE(valid_b64_chars(m1.album));
        REQUIRE(valid_b64_chars(m1.track));

        REQUIRE_FALSE(m1.cover.empty());
        std::string cover_str(m1.cover.begin(), m1.cover.end());
        REQUIRE(cover_str.find("<svg") != std::string::npos);
    }

    SECTION("string overload") {
        // Build a deterministic byte array and a base64 string (URL-safe, no padding)
        std::vector<uint8_t> bytes  = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};
        std::string          b64str = encode_b64_url(bytes);

        auto meta = IndexMetadata::extractMetadataFromIndex(b64str);

        REQUIRE_FALSE(meta.genre.empty());
        REQUIRE_FALSE(meta.artist.empty());
        REQUIRE_FALSE(meta.album.empty());
        REQUIRE_FALSE(meta.track.empty());

        REQUIRE(valid_b64_chars(meta.genre));
        REQUIRE(valid_b64_chars(meta.artist));
        REQUIRE(valid_b64_chars(meta.album));
        REQUIRE(valid_b64_chars(meta.track));

        REQUIRE_FALSE(meta.cover.empty());
        REQUIRE(meta.cover.find("<svg") != std::string::npos);
    }
}

TEST_CASE("IndexMetadata: string-overload malformed input handling", "[metadata][error][validation]") {
    // Create a valid small byte array and base64 string
    std::vector<uint8_t> bytes     = {0xDE, 0xAD, 0xBE, 0xEF};
    std::string          clean_b64 = encode_b64_url(bytes);

    // Inject some malformed characters into the base64 string
    std::string malformed = clean_b64;
    if (malformed.size() >= 2) {
        malformed.insert(1, "=");
        malformed.insert(malformed.size() - 1, "@");
    } else {
        malformed += "=@";
    }

    // Decoder should throw on malformed base64 input
    REQUIRE_THROWS_AS(IndexMetadata::extractMetadataFromIndex(malformed), std::invalid_argument);
}

TEST_CASE("IndexMetadata: generateSvgCover renders a pixel mosaic", "[metadata][svg][cover]") {
    std::vector<uint8_t> bytes = {0x12, 0x34, 0x56, 0x78};
    std::string          svg   = IndexMetadata::generateSvgCover(bytes, "t");

    // 16x16 grid of individually colored cells, not one flat background fill.
    size_t count = 0;
    size_t pos   = 0;
    while ((pos = svg.find("<rect", pos)) != std::string::npos) {
        ++count;
        pos += 5;
    }
    REQUIRE(count == 256 + 1); // 256 mosaic cells + 1 text backdrop panel
}

TEST_CASE("IndexMetadata: generateSvgCover is deterministic and byte-sensitive", "[metadata][svg][cover][determinism]") {
    std::vector<uint8_t> bytesA = {0x12, 0x34, 0x56, 0x78};
    std::vector<uint8_t> bytesB = {0xFF, 0xEE, 0xDD, 0xCC};

    std::string svgA1 = IndexMetadata::generateSvgCover(bytesA, "t");
    std::string svgA2 = IndexMetadata::generateSvgCover(bytesA, "t");
    std::string svgB  = IndexMetadata::generateSvgCover(bytesB, "t");

    // Same bytes -> same mosaic, every time.
    REQUIRE(svgA1 == svgA2);
    // Different bytes -> a different mosaic.
    REQUIRE(svgA1 != svgB);
}

TEST_CASE("IndexMetadata: generateSvgCover contains track text", "[metadata][svg][cover]") {
    std::vector<uint8_t> bytes = {0xFF, 0xEE, 0xDD};
    std::string          track = "MyTrack";
    std::string          svg   = IndexMetadata::generateSvgCover(bytes, track);

    REQUIRE(svg.find(track) != std::string::npos);
}

TEST_CASE("IndexMetadata: stress test across small and large cpp_int values", "[metadata][edge_case]") {
    // Edge cases with minimal indexes, plus progressively larger indexes.
    std::vector<cpp_int> values = {
        cpp_int(0),                                                                               // Zero
        cpp_int(1),                                                                               // One
        cpp_int(2),                                                                               // Two
        cpp_int(15),                                                                              // Small value
        cpp_int(255),                                                                             // Single byte max
        cpp_int(256),                                                                             // Just over single byte
        cpp_int(65535),                                                                           // Two bytes max (uint16_t max)
        cpp_int(65536),                                                                           // Just over two bytes
        cpp_int("4294967295"),                                                                    // 32-bit max
        cpp_int("18446744073709551615"),                                                          // 64-bit max
        cpp_int("340282366920938463463374607431768211455"),                                       // 128-bit value
        cpp_int("115792089237316195423570985008687907853269984665640564039457584007913129639935") // 256-bit value
    };

    for (const auto& idx : values) {
        INFO("Testing index: " << idx.convert_to<std::string>());
        auto meta = IndexMetadata::extractMetadataFromIndex(idx);

        // All fields should be non-empty
        REQUIRE_FALSE(meta.genre.empty());
        REQUIRE_FALSE(meta.artist.empty());
        REQUIRE_FALSE(meta.album.empty());
        REQUIRE_FALSE(meta.track.empty());

        // Every field's characters should be valid base64 URL-safe
        std::string recombined = meta.genre + meta.artist + meta.album + meta.track;
        REQUIRE(recombined.length() > 0);
        REQUIRE(valid_b64_chars(recombined));

        // Cover should be valid SVG
        REQUIRE_FALSE(meta.cover.empty());
        REQUIRE(meta.cover.find("<svg") != std::string::npos);
    }
}

TEST_CASE("IndexMetadata: empty index edge case", "[metadata][edge_case]") {
    // Test with zero index (minimal case)
    cpp_int idx  = 0;
    auto    meta = IndexMetadata::extractMetadataFromIndex(idx);

    // Should produce default metadata (not crash)
    REQUIRE_FALSE(meta.genre.empty());
    REQUIRE_FALSE(meta.artist.empty());
    REQUIRE_FALSE(meta.album.empty());
    REQUIRE_FALSE(meta.track.empty());

    // Cover should still be generated
    REQUIRE_FALSE(meta.cover.empty());
}
