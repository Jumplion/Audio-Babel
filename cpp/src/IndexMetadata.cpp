#include "../include/IndexMetadata.h"

#include <boost/multiprecision/cpp_int.hpp>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "../include/IndexNaming.h"
#include "../include/LibraryPosition.h"
#include "../include/Utilities.h"

namespace AudioBabel {

namespace {

    using boost::multiprecision::cpp_int;

    // Fixed permutation key for the cover-material Feistel. Distinct from
    // IndexNaming's NAME_KEY, IndexScramble's seed, and LibraryPosition's
    // POSITION_SALT so keying can never alias across features.
    constexpr uint64_t COVER_KEY = 0xB4B2A265B1A5C2EFULL;

    // Cover-material domain: exactly enough bits for the production cover's
    // pixel byte budget (pixelBytesNeeded(DEFAULT_CELL_SIZE) bytes). A whole
    // number of bytes is already an even bit count, so (unlike IndexNaming's
    // nameSpace(), whose D^4 name space needs rounding up) no ceiling step is
    // needed to get an even Feistel domain width.
    struct CoverSpace {
        cpp_int full; // modulus the index is folded against, deliberately NOT a power of two
        size_t  e;    // Feistel domain width in bits (== coverBits)
    };

    auto coverSpace() -> const CoverSpace& {
        static const CoverSpace cs = [] {
            const size_t coverBits = IndexMetadata::pixelBytesNeeded(IndexMetadata::DEFAULT_CELL_SIZE) * BITS_PER_BYTE;
            // Just under 2^coverBits and odd, so it's guaranteed not to be a
            // power of two itself. That matters: reduceModLarge's Horner-rule
            // fold only mixes every bit of the index into the residue when the
            // modulus isn't a power of two -- a power-of-two modulus would
            // degrade to a plain bitmask of the index's low bits, and sibling
            // library positions (which differ only in a scrambled low-order
            // offset -- see LibraryPosition.cpp) would then produce nearly
            // identical top-byte-dominated covers, same as the un-scrambled
            // byte dump this replaces.
            cpp_int full = (cpp_int(1) << coverBits) - 3;
            return CoverSpace{full, coverBits};
        }();
        return cs;
    }

    auto coverRoundKey(int round) -> uint64_t {
        uint64_t state = COVER_KEY ^ (0x9E3779B97F4A7C15ULL * (static_cast<uint64_t>(round) + 1));
        return AudioBabel::Utilities::splitmix64(state);
    }

    // Keyed bijection on [0, full) via feistelPow2 + cycle-walking (Black &
    // Rogaway, CT-RSA 2002): re-apply the power-of-two permutation until the
    // result lands back in [0, full). Mirrors IndexNaming's permuteEncode,
    // scaled to the cover's much wider material.
    auto permuteCoverEncode(const cpp_int& x) -> cpp_int {
        const CoverSpace& cs = coverSpace();
        cpp_int           y  = AudioBabel::Utilities::feistelPow2(x, cs.e, coverRoundKey, /*encrypt=*/true);
        while (y >= cs.full) {
            y = AudioBabel::Utilities::feistelPow2(y, cs.e, coverRoundKey, /*encrypt=*/true);
        }
        return y;
    }

    // Render `value` as exactly `n` bytes, most-significant first, left-padded
    // with zero bytes (not right-padded — value is a coverBits-bit number, so
    // any missing high-order bytes are genuinely leading zeros, not "ran out of
    // material" the way generateSvgCover's own nextByteOrZero padding means).
    auto toFixedBytesMsb(const cpp_int& value, size_t n) -> std::vector<uint8_t> {
        std::vector<uint8_t> raw;
        boost::multiprecision::export_bits(value, std::back_inserter(raw), BITS_PER_BYTE, true);
        std::vector<uint8_t> out(n, 0);
        const size_t         copyLen = std::min(raw.size(), n);
        std::copy(raw.end() - static_cast<std::ptrdiff_t>(copyLen), raw.end(), out.end() - static_cast<std::ptrdiff_t>(copyLen));
        return out;
    }

    // Derive the cover mosaic's pixel bytes from the whole index via a keyed
    // Feistel permutation -- the same treatment IndexNaming::namesForIndex
    // gives the genre/artist/album/track fields, and for the same reason:
    // sibling library positions differ only in a small low-order offset (see
    // LibraryPosition::reconstructIndexFromPosition), and reduceModLarge's
    // non-power-of-two fold plus this permutation scatters that difference
    // across every pixel instead of leaving neighbouring tracks/albums with
    // near-identical (or, for short indexes, mostly-black) cover art.
    auto coverBytesForIndex(const cpp_int& index) -> std::vector<uint8_t> {
        cpp_int idx = index < 0 ? cpp_int(0) : index;
        cpp_int m   = AudioBabel::Utilities::reduceModLarge(idx, coverSpace().full);
        cpp_int s   = permuteCoverEncode(m);
        return toFixedBytesMsb(s, IndexMetadata::pixelBytesNeeded(IndexMetadata::DEFAULT_CELL_SIZE));
    }

} // namespace

auto IndexMetadata::extractMetadataFromIndex(const boost::multiprecision::cpp_int& index) -> IndexMetadata {
    std::vector<uint8_t> bytes = coverBytesForIndex(index);

    LibraryPosition position = calculateLibraryPosition(index);
    return buildMetadataFromBytesAndPosition(bytes, position, IndexNaming::namesForIndex(index));
}

// String overload: extract metadata directly from a bijective base-64 index string
auto IndexMetadata::extractMetadataFromIndex(const std::string& base64Index) -> IndexMetadata {
    // Validate that the string only uses the URL-safe alphabet.
    if (!::AudioBabel::Utilities::isValidBase64Url(base64Index)) {
        throw std::invalid_argument("Invalid base64 URL-safe string provided to extractMetadataFromIndex");
    }

    boost::multiprecision::cpp_int index = ::AudioBabel::Utilities::b64ToIndex(base64Index);

    std::vector<uint8_t> bytes = coverBytesForIndex(index);

    LibraryPosition position = calculateLibraryPosition(index);
    return buildMetadataFromBytesAndPosition(bytes, position, IndexNaming::namesForIndex(index));
}

auto IndexMetadata::buildMetadataFromBytesAndPosition(const std::vector<uint8_t>& bytes,
                                                      const LibraryPosition&      position,
                                                      const IndexNaming::Names&   names) -> IndexMetadata {
    IndexMetadata meta;
    meta.position = position;

    meta.genre  = names.genre;
    meta.artist = names.artist;
    meta.album  = names.album;
    meta.track  = names.track;

    meta.cover = IndexMetadata::generateSvgCover(bytes, meta.track);
    return meta;
}

namespace {

    // Next byte of `bytes` at `cursor`, or 0 once the supply runs out — lets
    // short/zero indexes still render a (black-padded) mosaic instead of
    // needing a special case.
    auto nextByteOrZero(const std::vector<uint8_t>& bytes, size_t& cursor) -> uint8_t {
        uint8_t v = cursor < bytes.size() ? bytes[cursor] : 0;
        ++cursor;
        return v;
    }

    auto appendLE16(std::vector<uint8_t>& out, uint16_t v) -> void {
        out.push_back(static_cast<uint8_t>(v & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    auto appendLE32(std::vector<uint8_t>& out, uint32_t v) -> void {
        out.push_back(static_cast<uint8_t>(v & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }

    // Minimal uncompressed 24-bit-per-pixel BMP: BITMAPFILEHEADER (14 bytes)
    // + BITMAPINFOHEADER (40 bytes) + bottom-up, BGR, row-padded-to-4-bytes
    // pixel data. No color table, no compression -- deliberately: this skips
    // needing a DEFLATE/zlib dependency (as PNG would require) while still
    // being a real raster image, not vector markup.
    auto buildBmp24(const std::vector<uint8_t>& rgbTopDown, unsigned width, unsigned height) -> std::vector<uint8_t> {
        const unsigned rowBytes  = width * 3;
        const unsigned rowPadded = (rowBytes + 3) & ~0x3U;
        const uint32_t imageSize = rowPadded * height;
        const uint32_t fileSize  = 14 + 40 + imageSize;

        std::vector<uint8_t> bmp;
        bmp.reserve(fileSize);

        // BITMAPFILEHEADER
        bmp.push_back('B');
        bmp.push_back('M');
        appendLE32(bmp, fileSize);
        appendLE32(bmp, 0);       // reserved
        appendLE32(bmp, 14 + 40); // pixel data offset

        // BITMAPINFOHEADER
        appendLE32(bmp, 40); // header size
        appendLE32(bmp, width);
        appendLE32(bmp, height); // positive height => bottom-up row order
        appendLE16(bmp, 1);      // color planes
        appendLE16(bmp, 24);     // bits per pixel
        appendLE32(bmp, 0);      // BI_RGB, uncompressed
        appendLE32(bmp, imageSize);
        appendLE32(bmp, 0); // x pixels/meter (unspecified)
        appendLE32(bmp, 0); // y pixels/meter (unspecified)
        appendLE32(bmp, 0); // colors used
        appendLE32(bmp, 0); // important colors

        // Pixel data, bottom row first, BGR per pixel (BMP convention).
        const unsigned padBytes = rowPadded - rowBytes;
        for (unsigned row = height; row-- > 0;) {
            size_t rowStart = static_cast<size_t>(row) * width * 3;
            for (unsigned col = 0; col < width; ++col) {
                size_t px = rowStart + static_cast<size_t>(col) * 3;
                bmp.push_back(rgbTopDown[px + 2]); // B
                bmp.push_back(rgbTopDown[px + 1]); // G
                bmp.push_back(rgbTopDown[px + 0]); // R
            }
            for (unsigned p = 0; p < padBytes; ++p) {
                bmp.push_back(0);
            }
        }
        return bmp;
    }

    // Standard (RFC 4648) padded base64 -- the alphabet `data:` URIs require.
    // Distinct from Utilities::indexToB64, which uses a URL-safe, unpadded
    // alphabet for encoding the bijective index itself, not arbitrary bytes.
    auto base64EncodeStandard(const std::vector<uint8_t>& bytes) -> std::string {
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string       out;
        out.reserve(((bytes.size() + 2) / 3) * 4);

        size_t i = 0;
        for (; i + 3 <= bytes.size(); i += 3) {
            uint32_t n = (static_cast<uint32_t>(bytes[i]) << 16) | (static_cast<uint32_t>(bytes[i + 1]) << 8) | bytes[i + 2];
            out.push_back(alphabet[(n >> 18) & 0x3F]);
            out.push_back(alphabet[(n >> 12) & 0x3F]);
            out.push_back(alphabet[(n >> 6) & 0x3F]);
            out.push_back(alphabet[n & 0x3F]);
        }
        size_t remaining = bytes.size() - i;
        if (remaining == 1) {
            uint32_t n = static_cast<uint32_t>(bytes[i]) << 16;
            out.push_back(alphabet[(n >> 18) & 0x3F]);
            out.push_back(alphabet[(n >> 12) & 0x3F]);
            out.push_back('=');
            out.push_back('=');
        } else if (remaining == 2) {
            uint32_t n = (static_cast<uint32_t>(bytes[i]) << 16) | (static_cast<uint32_t>(bytes[i + 1]) << 8);
            out.push_back(alphabet[(n >> 18) & 0x3F]);
            out.push_back(alphabet[(n >> 12) & 0x3F]);
            out.push_back(alphabet[(n >> 6) & 0x3F]);
            out.push_back('=');
        }
        return out;
    }

} // namespace

auto IndexMetadata::generateSvgCover(const std::vector<uint8_t>& bytes, const std::string& track, unsigned cellSize) -> std::string {
    // Build the native-resolution pixel buffer directly from bytes: each
    // tile's (R, G, B) is the next three bytes of `bytes`, in reading order
    // -- a direct, bijective byte-to-pixel dump, not a PRNG stream. The same
    // index always renders the same cover (still deterministic), and the
    // mapping is invertible: packing a target image's quantized bytes at
    // this same offset makes an index decode to that exact cover.
    unsigned             cellsPerSide = cellSize > 0 ? CANVAS_SIZE / cellSize : 1;
    std::vector<uint8_t> rgb(static_cast<size_t>(cellsPerSide) * cellsPerSide * 3);
    size_t               cursor = 0;
    for (auto& channel : rgb) {
        channel = nextByteOrZero(bytes, cursor);
    }

    // Embed the native-resolution bitmap as a single raster <image>, scaled
    // up to the 256x256 canvas with nearest-neighbor ("pixelated") sampling
    // so tile edges stay crisp -- visually identical to drawing cellSize x
    // cellSize <rect> tiles, but at a small fraction of the markup size.
    std::string bmpBase64 = base64EncodeStandard(buildBmp24(rgb, cellsPerSide, cellsPerSide));

    std::string svg = "<svg xmlns='http://www.w3.org/2000/svg' width='256' height='256' viewBox='0 0 256 256'>";
    svg += "<image x='0' y='0' width='256' height='256' image-rendering='pixelated' href='data:image/bmp;base64,";
    svg += bmpBase64;
    svg += "'/>";

    // Legibility backdrop: the mosaic below can be light in places, so the
    // track label gets its own translucent panel rather than relying on
    // contrast with whatever tile colors land behind it.
    svg += "<rect x='8' y='112' width='240' height='32' rx='4' fill='#000' fill-opacity='0.55'/>";
    svg += "<text x='50%' y='128' font-size='20' text-anchor='middle' fill='#fff' dominant-baseline='middle'>";
    svg += track;
    svg += "</text></svg>";
    return svg;
}

} // namespace AudioBabel
