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

    // Number of leading (most-significant) index bytes the SVG cover mosaic seeds from.
    constexpr size_t COVER_SEED_BYTES = 8;

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
    std::vector<uint8_t> bytes = topBytesMsb(index, COVER_SEED_BYTES);

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

    std::vector<uint8_t> bytes = topBytesMsb(index, COVER_SEED_BYTES);

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

    // The 256x256 canvas is subdivided into GRID_SIZE x GRID_SIZE square cells,
    // each individually colored — a pixel mosaic instead of a flat fill. Bump
    // GRID_SIZE for a finer mosaic; CELL_SIZE follows automatically.
    constexpr unsigned CANVAS_SIZE = 256;
    constexpr unsigned GRID_SIZE   = 16;
    constexpr unsigned CELL_SIZE   = CANVAS_SIZE / GRID_SIZE;

    auto appendHexColor(std::string& svg, uint64_t rgb) -> void {
        const char* hex = "0123456789abcdef";
        for (int i = 5; i >= 0; --i) {
            svg.push_back(hex[(rgb >> (i * 4)) & 0xF]);
        }
    }

} // namespace

auto IndexMetadata::generateSvgCover(const std::vector<uint8_t>& bytes, const std::string& track) -> std::string {
    // Seed a splitmix64 stream from the index's seed bytes (same avalanche
    // mixer IndexNaming/IndexScramble use), then draw one cell's color per
    // stream output — so the whole mosaic is a deterministic function of the
    // index: the same index always renders the same cover.
    uint64_t state = 0;
    for (uint8_t b : bytes) {
        ::AudioBabel::Utilities::mixIn(state, b);
    }

    std::string svg = "<svg xmlns='http://www.w3.org/2000/svg' width='256' height='256' viewBox='0 0 256 256'>";
    for (unsigned row = 0; row < GRID_SIZE; ++row) {
        for (unsigned col = 0; col < GRID_SIZE; ++col) {
            uint64_t rgb = ::AudioBabel::Utilities::splitmix64(state);
            svg += "<rect x='" + std::to_string(col * CELL_SIZE) + "' y='" + std::to_string(row * CELL_SIZE) + "' width='" +
                   std::to_string(CELL_SIZE) + "' height='" + std::to_string(CELL_SIZE) + "' fill='#";
            appendHexColor(svg, rgb);
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
