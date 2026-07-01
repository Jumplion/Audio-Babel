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

// --- Independent standard-base64 + BMP decoders, used only to verify
// generateSvgCover's embedded image. Deliberately separate from the
// production encoder in IndexMetadata.cpp so these tests exercise a real
// round-trip rather than checking the implementation against itself.

static std::vector<uint8_t> decode_b64_standard(const std::string& s) {
    auto valueOf = [](char c) -> int {
        if (c >= 'A' && c <= 'Z')
            return c - 'A';
        if (c >= 'a' && c <= 'z')
            return c - 'a' + 26;
        if (c >= '0' && c <= '9')
            return c - '0' + 52;
        if (c == '+')
            return 62;
        if (c == '/')
            return 63;
        return -1; // padding ('=') or terminator
    };
    std::vector<uint8_t> out;
    uint32_t             acc  = 0;
    int                  bits = 0;
    for (char c : s) {
        int v = valueOf(c);
        if (v < 0)
            break;
        acc = (acc << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((acc >> bits) & 0xFF));
        }
    }
    return out;
}

// Extracts the base64 payload between `base64,` and the closing `'` from
// generated SVG markup's embedded <image href='data:image/bmp;base64,...'>.
static std::string extract_bmp_base64(const std::string& svg) {
    static const std::string marker = "base64,";
    size_t                   start  = svg.find(marker);
    REQUIRE(start != std::string::npos);
    start += marker.size();
    size_t end = svg.find('\'', start);
    REQUIRE(end != std::string::npos);
    return svg.substr(start, end - start);
}

static uint32_t read_le32_at(const std::vector<uint8_t>& b, size_t off) {
    return static_cast<uint32_t>(b[off]) | (static_cast<uint32_t>(b[off + 1]) << 8) | (static_cast<uint32_t>(b[off + 2]) << 16) |
           (static_cast<uint32_t>(b[off + 3]) << 24);
}

struct DecodedBmp {
    unsigned             width;
    unsigned             height;
    std::vector<uint8_t> rgbTopDown; // row-major, top-down, RGB per pixel
};

static DecodedBmp decode_bmp24(const std::vector<uint8_t>& bmp) {
    REQUIRE(bmp.size() >= 54);
    REQUIRE(bmp[0] == 'B');
    REQUIRE(bmp[1] == 'M');
    uint32_t pixelOffset = read_le32_at(bmp, 10);
    uint32_t headerSize  = read_le32_at(bmp, 14);
    REQUIRE(headerSize == 40);
    uint32_t width  = read_le32_at(bmp, 18);
    uint32_t height = read_le32_at(bmp, 22);
    auto     bpp    = static_cast<uint16_t>(bmp[28] | (bmp[29] << 8));
    REQUIRE(bpp == 24);

    unsigned rowBytes  = width * 3;
    unsigned rowPadded = (rowBytes + 3) & ~0x3U;

    DecodedBmp result;
    result.width  = width;
    result.height = height;
    result.rgbTopDown.resize(static_cast<size_t>(width) * height * 3);

    for (unsigned row = 0; row < height; ++row) {
        // BMP rows are stored bottom-up; convert to top-down while decoding.
        unsigned srcRow   = height - 1 - row;
        size_t   rowStart = pixelOffset + static_cast<size_t>(srcRow) * rowPadded;
        for (unsigned col = 0; col < width; ++col) {
            size_t srcPx                 = rowStart + static_cast<size_t>(col) * 3;
            size_t dstPx                 = (static_cast<size_t>(row) * width + col) * 3;
            result.rgbTopDown[dstPx + 0] = bmp[srcPx + 2]; // R (BMP stores B, G, R)
            result.rgbTopDown[dstPx + 1] = bmp[srcPx + 1]; // G
            result.rgbTopDown[dstPx + 2] = bmp[srcPx + 0]; // B
        }
    }
    return result;
}

static DecodedBmp decode_cover_bitmap(const std::string& svg) {
    return decode_bmp24(decode_b64_standard(extract_bmp_base64(svg)));
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

TEST_CASE("IndexMetadata: generateSvgCover embeds a single raster image, not per-tile markup", "[metadata][svg][cover]") {
    std::vector<uint8_t> bytes = {0x12, 0x34, 0x56, 0x78};
    std::string          svg   = IndexMetadata::generateSvgCover(bytes, "t");

    // One embedded bitmap carries the whole mosaic; only the text backdrop
    // panel is still drawn as a <rect> -- not one <rect> per tile.
    size_t imageCount = 0;
    size_t pos        = 0;
    while ((pos = svg.find("<image", pos)) != std::string::npos) {
        ++imageCount;
        pos += 6;
    }
    REQUIRE(imageCount == 1);

    size_t rectCount = 0;
    pos              = 0;
    while ((pos = svg.find("<rect", pos)) != std::string::npos) {
        ++rectCount;
        pos += 5;
    }
    REQUIRE(rectCount == 1); // text backdrop panel only
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

TEST_CASE("IndexMetadata: generateSvgCover reads pixels directly, no PRNG mixing", "[metadata][svg][cover][bijective]") {
    // cellSize=128 -> (256/128)^2 = 2x2 = 4 tiles. Decoding the embedded
    // bitmap should show exactly the bytes supplied, verbatim -- proof
    // there's no hashing/PRNG step between bytes and pixels.
    std::vector<uint8_t> bytes = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    std::string          svg   = IndexMetadata::generateSvgCover(bytes, "t", 128);

    DecodedBmp decoded = decode_cover_bitmap(svg);
    REQUIRE(decoded.width == 2);
    REQUIRE(decoded.height == 2);

    // Tile 0 (top-left): bytes[0..2]
    REQUIRE(decoded.rgbTopDown[0] == 0x12);
    REQUIRE(decoded.rgbTopDown[1] == 0x34);
    REQUIRE(decoded.rgbTopDown[2] == 0x56);
    // Tile 1 (top-right): bytes[3..5]
    REQUIRE(decoded.rgbTopDown[3] == 0x78);
    REQUIRE(decoded.rgbTopDown[4] == 0x9A);
    REQUIRE(decoded.rgbTopDown[5] == 0xBC);
}

TEST_CASE("IndexMetadata: generateSvgCover pads missing cells with black", "[metadata][svg][cover][bijective]") {
    // Fewer bytes than pixelBytesNeeded(cellSize) -- the uncovered tiles
    // should render black rather than throwing or reading out of bounds.
    std::vector<uint8_t> bytes = {0xAA, 0xBB}; // cellSize=128 -> 2x2 tiles need 12 bytes; only 2 supplied
    std::string          svg   = IndexMetadata::generateSvgCover(bytes, "t", 128);

    DecodedBmp decoded = decode_cover_bitmap(svg);

    // Tile 0: 0xAA, 0xBB, then padded 0x00
    REQUIRE(decoded.rgbTopDown[0] == 0xAA);
    REQUIRE(decoded.rgbTopDown[1] == 0xBB);
    REQUIRE(decoded.rgbTopDown[2] == 0x00);

    // Last tile (bottom-right), entirely past the supplied bytes: fully black.
    size_t lastPx = decoded.rgbTopDown.size() - 3;
    REQUIRE(decoded.rgbTopDown[lastPx + 0] == 0x00);
    REQUIRE(decoded.rgbTopDown[lastPx + 1] == 0x00);
    REQUIRE(decoded.rgbTopDown[lastPx + 2] == 0x00);
}

TEST_CASE("IndexMetadata: generateSvgCover honors cellSize", "[metadata][svg][cover]") {
    // cellSize=64 -> (256/64)^2 = 4x4 = 16 tiles.
    std::vector<uint8_t> bytes(IndexMetadata::pixelBytesNeeded(64), 0x7F);

    std::string svg     = IndexMetadata::generateSvgCover(bytes, "t", 64);
    DecodedBmp  decoded = decode_cover_bitmap(svg);

    REQUIRE(decoded.width == 4);
    REQUIRE(decoded.height == 4);
    for (uint8_t channel : decoded.rgbTopDown) {
        REQUIRE(channel == 0x7F);
    }
}

TEST_CASE("IndexMetadata: extractMetadataFromIndex covers render at DEFAULT_CELL_SIZE resolution", "[metadata][svg][cover]") {
    cpp_int idx  = cpp_int("123456789012345678901234567890");
    auto    meta = IndexMetadata::extractMetadataFromIndex(idx);

    DecodedBmp decoded      = decode_cover_bitmap(meta.cover);
    unsigned   expectedSide = 256 / IndexMetadata::DEFAULT_CELL_SIZE;
    REQUIRE(decoded.width == expectedSide);
    REQUIRE(decoded.height == expectedSide);
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
