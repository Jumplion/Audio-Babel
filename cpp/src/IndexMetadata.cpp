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

    // Number of leading (most-significant) index bytes the SVG cover color uses.
    constexpr size_t COVER_COLOR_BYTES = 3;

    // The most-significant `n` bytes of `index`, MSB-first — the same prefix
    // export_bits(msv=true) would yield, but without allocating the whole
    // integer's byte string just to read its top three bytes (an O(N) waste on
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
    std::vector<uint8_t> bytes = topBytesMsb(index, COVER_COLOR_BYTES);

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

    std::vector<uint8_t> bytes = topBytesMsb(index, COVER_COLOR_BYTES);

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

auto IndexMetadata::generateSvgCover(const std::vector<uint8_t>& bytes, const std::string& track) -> std::string {
    std::string svg = "<svg xmlns='http://www.w3.org/2000/svg' width='256' height='256'>";
    svg += "<rect width='100%' height='100%' fill='#";
    unsigned int color = 0;
    for (size_t i = 0; i < 3; ++i) {
        color = (color << 8) | (i < bytes.size() ? bytes[i] : 0);
    }
    const char* hex = "0123456789abcdef";
    for (int i = 5; i >= 0; --i) {
        unsigned int nib = (color >> (i * 4)) & 0xF;
        svg.push_back(hex[nib]);
    }
    svg += "'/><text x='50%' y='50%' font-size='20' text-anchor='middle' fill='#fff' dominant-baseline='middle'>";
    svg += track;
    svg += "</text></svg>";
    return svg;
}

} // namespace AudioBabel
