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
    std::vector<uint8_t> bytes = topBytesMsb(index, pixelBytesNeeded(DEFAULT_GRID_SIZE));

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

    std::vector<uint8_t> bytes = topBytesMsb(index, pixelBytesNeeded(DEFAULT_GRID_SIZE));

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

    // The 256x256 canvas is subdivided into gridSize x gridSize square cells,
    // each individually colored — a pixel mosaic instead of a flat fill.
    constexpr unsigned CANVAS_SIZE = 256;

    auto appendHexByte(std::string& svg, uint8_t v) -> void {
        const char* hex = "0123456789abcdef";
        svg.push_back(hex[(v >> 4) & 0xF]);
        svg.push_back(hex[v & 0xF]);
    }

    // Next byte of `bytes` at `cursor`, or 0 once the supply runs out — lets
    // short/zero indexes still render a (black-padded) mosaic instead of
    // needing a special case.
    auto nextByteOrZero(const std::vector<uint8_t>& bytes, size_t& cursor) -> uint8_t {
        uint8_t v = cursor < bytes.size() ? bytes[cursor] : 0;
        ++cursor;
        return v;
    }

} // namespace

auto IndexMetadata::generateSvgCover(const std::vector<uint8_t>& bytes, const std::string& track, unsigned gridSize) -> std::string {
    // Each cell reads its (R, G, B) straight off the next three bytes of
    // `bytes` — a direct, bijective byte-to-pixel dump, not a PRNG stream.
    // The same index always renders the same cover (still deterministic),
    // but now the mapping is invertible: packing a target image's quantized
    // bytes at this same offset makes an index decode to that exact cover.
    unsigned cellSize = gridSize > 0 ? CANVAS_SIZE / gridSize : CANVAS_SIZE;

    std::string svg = "<svg xmlns='http://www.w3.org/2000/svg' width='256' height='256' viewBox='0 0 256 256'>";
    size_t      cursor = 0;
    for (unsigned row = 0; row < gridSize; ++row) {
        for (unsigned col = 0; col < gridSize; ++col) {
            uint8_t r = nextByteOrZero(bytes, cursor);
            uint8_t g = nextByteOrZero(bytes, cursor);
            uint8_t b = nextByteOrZero(bytes, cursor);
            svg += "<rect x='" + std::to_string(col * cellSize) + "' y='" + std::to_string(row * cellSize) + "' width='" +
                   std::to_string(cellSize) + "' height='" + std::to_string(cellSize) + "' fill='#";
            appendHexByte(svg, r);
            appendHexByte(svg, g);
            appendHexByte(svg, b);
            svg += "'/>";
        }
    }

    // Legibility backdrop: the mosaic below can be light in places, so the
    // track label gets its own translucent panel rather than relying on
    // contrast with whatever cell colors land behind it.
    svg += "<rect x='8' y='112' width='240' height='32' rx='4' fill='#000' fill-opacity='0.55'/>";
    svg += "<text x='50%' y='128' font-size='20' text-anchor='middle' fill='#fff' dominant-baseline='middle'>";
    svg += track;
    svg += "</text></svg>";
    return svg;
}

} // namespace AudioBabel
