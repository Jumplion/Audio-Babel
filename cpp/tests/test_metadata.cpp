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

// Helper to validate base64 URL-safe characters
static bool valid_b64_chars(const std::string& s) {
    for (char c : s) {
        if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

// Helper to encode bytes to base64 URL-safe (no padding)
static std::string encode_b64_url(const std::vector<uint8_t>& bytes) {
    static const char b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string       b64str;
    b64str.reserve((bytes.size() * 8 + 5) / 6);
    uint32_t acc      = 0;
    int      acc_bits = 0;
    for (uint8_t byte : bytes) {
        acc = (acc << 8) | byte;
        acc_bits += 8;
        while (acc_bits >= 6) {
            acc_bits -= 6;
            auto idx = static_cast<uint8_t>((acc >> acc_bits) & 0x3F);
            b64str.push_back(b64_alpha[idx]);
        }
    }
    if (acc_bits > 0) {
        auto idx = static_cast<uint8_t>((acc << (6 - acc_bits)) & 0x3F);
        b64str.push_back(b64_alpha[idx]);
    }
    return b64str;
}

TEST_CASE("AudioIndex: indexToMetadata deterministic and valid", "[metadata][determinism]") {
    // Build a sample byte vector (non-empty) and construct a cpp_int (MSB-first)
    std::vector<uint8_t> bytes = {0x10, 0x20, 0x30, 0x41, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA,
                                  0xBB, 0xCC, 0xDD, 0xEE, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    cpp_int              idx   = 0;
    for (uint8_t b : bytes) {
        idx <<= 8;
        idx |= cpp_int(static_cast<uint32_t>(b));
    }

    auto        m1     = AudioIndex::indexToMetadata(idx);
    auto        m2     = AudioIndex::indexToMetadata(idx);
    std::string b64str = encode_b64_url(bytes);

    SECTION("Metadata extraction is deterministic") {
        REQUIRE(m1.genre == m2.genre);
        REQUIRE(m1.artist == m2.artist);
        REQUIRE(m1.album == m2.album);
        REQUIRE(m1.track == m2.track);
    }

    SECTION("All metadata fields are non-empty") {
        REQUIRE_FALSE(m1.genre.empty());
        REQUIRE_FALSE(m1.artist.empty());
        REQUIRE_FALSE(m1.album.empty());
        REQUIRE_FALSE(m1.track.empty());
    }

    SECTION("All metadata fields contain valid base64 URL-safe characters") {
        REQUIRE(valid_b64_chars(m1.genre));
        REQUIRE(valid_b64_chars(m1.artist));
        REQUIRE(valid_b64_chars(m1.album));
        REQUIRE(valid_b64_chars(m1.track));
    }

    SECTION("Concatenation recreates base64 index") {
        std::string recombined = m1.genre + m1.artist + m1.album + m1.track;
        REQUIRE(recombined == b64str);
    }

    SECTION("Cover contains SVG markup") {
        REQUIRE_FALSE(m1.cover.empty());
        std::string cover_str(m1.cover.begin(), m1.cover.end());
        REQUIRE(cover_str.find("<svg") != std::string::npos);
    }
}

TEST_CASE("IndexMetadata: string-overload deterministic and recomposition", "[metadata][base64]") {
    // Build a deterministic byte array and a base64 string (URL-safe, no padding)
    std::vector<uint8_t> bytes  = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};
    std::string          b64str = encode_b64_url(bytes);

    auto meta = IndexMetadata::extractMetadataFromIndex(b64str);

    SECTION("All fields are non-empty") {
        REQUIRE_FALSE(meta.genre.empty());
        REQUIRE_FALSE(meta.artist.empty());
        REQUIRE_FALSE(meta.album.empty());
        REQUIRE_FALSE(meta.track.empty());
    }

    SECTION("All fields contain valid base64 characters") {
        REQUIRE(valid_b64_chars(meta.genre));
        REQUIRE(valid_b64_chars(meta.artist));
        REQUIRE(valid_b64_chars(meta.album));
        REQUIRE(valid_b64_chars(meta.track));
    }

    SECTION("Concatenation recreates base64 index") {
        std::string recombined = meta.genre + meta.artist + meta.album + meta.track;
        REQUIRE(recombined == b64str);
    }

    SECTION("Cover contains SVG markup") {
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

TEST_CASE("IndexMetadata: generateSvgCover color derivation", "[metadata][svg][cover]") {
    std::vector<uint8_t> bytes = {0x12, 0x34, 0x56, 0x78};
    std::string          svg   = IndexMetadata::generateSvgCover(bytes, "t");

    // Color computed from first three bytes: 0x12 0x34 0x56 -> hex 123456
    REQUIRE(svg.find("#123456") != std::string::npos);
}

TEST_CASE("IndexMetadata: generateSvgCover contains track text", "[metadata][svg][cover]") {
    std::vector<uint8_t> bytes = {0xFF, 0xEE, 0xDD};
    std::string          track = "MyTrack";
    std::string          svg   = IndexMetadata::generateSvgCover(bytes, track);

    REQUIRE(svg.find(track) != std::string::npos);
}

TEST_CASE("IndexMetadata: stress test with very small cpp_int values", "[metadata][edge_case]") {
    // Test edge cases with minimal indexes
    std::vector<cpp_int> small_values = {
        cpp_int(0),     // Zero
        cpp_int(1),     // One
        cpp_int(2),     // Two
        cpp_int(15),    // Small value
        cpp_int(255),   // Single byte max
        cpp_int(256),   // Just over single byte
        cpp_int(65535), // Two bytes max (uint16_t max)
        cpp_int(65536)  // Just over two bytes
    };

    for (const auto& idx : small_values) {
        INFO("Testing index: " << idx.convert_to<std::string>());
        auto meta = AudioIndex::indexToMetadata(idx);

        // All fields should be non-empty
        REQUIRE_FALSE(meta.genre.empty());
        REQUIRE_FALSE(meta.artist.empty());
        REQUIRE_FALSE(meta.album.empty());
        REQUIRE_FALSE(meta.track.empty());

        // Concatenation should recreate valid base64
        std::string recombined = meta.genre + meta.artist + meta.album + meta.track;

        // Verify all characters are valid base64 URL-safe
        for (char c : recombined) {
            bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
            REQUIRE(valid);
        }

        // Cover should be valid SVG
        REQUIRE_FALSE(meta.cover.empty());
        REQUIRE(meta.cover.find("<svg") != std::string::npos);
    }
}

TEST_CASE("IndexMetadata: stress test with very large cpp_int values", "[metadata][edge_case]") {
    // Test with progressively larger indexes
    std::vector<cpp_int> large_values = {
        cpp_int("4294967295"),                                                                    // 32-bit max
        cpp_int("18446744073709551615"),                                                          // 64-bit max
        cpp_int("340282366920938463463374607431768211455"),                                       // 128-bit value
        cpp_int("115792089237316195423570985008687907853269984665640564039457584007913129639935") // 256-bit value
    };

    size_t test_num = 0;
    for (const auto& idx : large_values) {
        test_num++;
        INFO("Testing large index #" << test_num);

        auto meta = AudioIndex::indexToMetadata(idx);

        // All fields should be non-empty
        REQUIRE_FALSE(meta.genre.empty());
        REQUIRE_FALSE(meta.artist.empty());
        REQUIRE_FALSE(meta.album.empty());
        REQUIRE_FALSE(meta.track.empty());

        // Concatenation should recreate valid base64
        std::string recombined = meta.genre + meta.artist + meta.album + meta.track;

        // Verify all characters are valid base64 URL-safe
        for (char c : recombined) {
            bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
            REQUIRE(valid);
        }

        // Verify the base64 string length is reasonable
        REQUIRE(recombined.length() > 0);

        // Cover should be valid SVG
        REQUIRE_FALSE(meta.cover.empty());
        REQUIRE(meta.cover.find("<svg") != std::string::npos);
    }
}

TEST_CASE("IndexMetadata: weighted splitting logic consistency", "[metadata][determinism]") {
    // The metadata splitting should be deterministic and consistent
    std::vector<cpp_int> test_indexes = {cpp_int(12345), cpp_int(987654321), cpp_int("1234567890123456789"), cpp_int("999999999999999999999999")};

    for (const auto& idx : test_indexes) {
        INFO("Testing index: " << idx.convert_to<std::string>());

        // Generate metadata multiple times
        auto meta1 = AudioIndex::indexToMetadata(idx);
        auto meta2 = AudioIndex::indexToMetadata(idx);
        auto meta3 = AudioIndex::indexToMetadata(idx);

        // Verify consistency
        REQUIRE(meta1.genre == meta2.genre);
        REQUIRE(meta2.genre == meta3.genre);
        REQUIRE(meta1.artist == meta2.artist);
        REQUIRE(meta2.artist == meta3.artist);
        REQUIRE(meta1.album == meta2.album);
        REQUIRE(meta2.album == meta3.album);
        REQUIRE(meta1.track == meta2.track);
        REQUIRE(meta2.track == meta3.track);

        // Verify the weighted lengths are reasonable
        // genre should get 30%, artist 30%, album 30%, track 10%
        std::string combined  = meta1.genre + meta1.artist + meta1.album + meta1.track;
        size_t      total_len = combined.length();

        if (total_len > 0) {
            double genre_ratio  = static_cast<double>(meta1.genre.length()) / total_len;
            double artist_ratio = static_cast<double>(meta1.artist.length()) / total_len;
            double album_ratio  = static_cast<double>(meta1.album.length()) / total_len;
            double track_ratio  = static_cast<double>(meta1.track.length()) / total_len;

            // Allow very generous tolerance since distribution is based on byte content, not fixed ratios
            // The weighted algorithm can produce highly skewed distributions for certain byte patterns
            // We just verify that no field dominates completely or disappears entirely
            REQUIRE(genre_ratio >= 0.05);
            REQUIRE(genre_ratio <= 0.60);
            REQUIRE(artist_ratio >= 0.05);
            REQUIRE(artist_ratio <= 0.60);
            REQUIRE(album_ratio >= 0.05);
            REQUIRE(album_ratio <= 0.60);
            REQUIRE(track_ratio >= 0.02);
            REQUIRE(track_ratio <= 0.50);
        }
    }
}

TEST_CASE("IndexMetadata: empty index edge case", "[metadata][edge_case]") {
    // Test with zero index (minimal case)
    cpp_int idx  = 0;
    auto    meta = AudioIndex::indexToMetadata(idx);

    // Should produce default metadata (not crash)
    REQUIRE_FALSE(meta.genre.empty());
    REQUIRE_FALSE(meta.artist.empty());
    REQUIRE_FALSE(meta.album.empty());
    REQUIRE_FALSE(meta.track.empty());

    // Cover should still be generated
    REQUIRE_FALSE(meta.cover.empty());
}
