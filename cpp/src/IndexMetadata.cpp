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

    // The most-significant `n` bytes of `index`, MSB-first — the same prefix
    // export_bits(msv=true) would yield, but without allocating the whole
    // integer's byte string just to read its top few bytes (an O(N) waste on
    // large indices). Returns fewer than `n` bytes when the index is shorter.
    auto topBytesMsb(const boost::multiprecision::cpp_int& index, size_t n) -> std::vector<uint8_t> {
        std::vector<uint8_t> bytes;
        if (index <= 0) {
            return bytes;
        }
        boost::multiprecision::cpp_int v          = index;
        size_t                         totalBytes = static_cast<size_t>(boost::multiprecision::msb(index) / BITS_PER_BYTE) + 1;
        if (totalBytes > n) {
            v >>= (totalBytes - n) * BITS_PER_BYTE; // drop the low bytes the cover never reads
        }
        boost::multiprecision::export_bits(v, std::back_inserter(bytes), BITS_PER_BYTE, true);
        return bytes;
    }

} // namespace

auto IndexMetadata::extractMetadataFromIndex(const boost::multiprecision::cpp_int& index) -> IndexMetadata {
    std::vector<uint8_t> bytes = topBytesMsb(index, pixelBytesNeeded(DEFAULT_CELL_SIZE));

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

    std::vector<uint8_t> bytes = topBytesMsb(index, pixelBytesNeeded(DEFAULT_CELL_SIZE));

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
        appendLE32(bmp, 0);      // reserved
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
        std::string        out;
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
    unsigned              cellsPerSide = cellSize > 0 ? CANVAS_SIZE / cellSize : 1;
    std::vector<uint8_t>  rgb(static_cast<size_t>(cellsPerSide) * cellsPerSide * 3);
    size_t                cursor = 0;
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
