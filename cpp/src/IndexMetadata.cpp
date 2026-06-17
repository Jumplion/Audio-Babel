#include "../include/IndexMetadata.h"

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "../include/LibraryPosition.h"
#include "../include/Utilities.h"

namespace AudioBabel {

// Metadata is purely cosmetic. It is derived from the bijective base-64 index
// string (and the per-character alphabet values) so it stays consistent with
// the single string encoding used across the system.

// Turn a bijective base-64 index string into a vector of its per-character
// alphabet values (0..63). Used only to seed the cosmetic weighting / cover.
static auto b64StringToValues(const std::string& b64str) -> std::vector<uint8_t> {
    std::vector<uint8_t> values;
    values.reserve(b64str.size());
    for (char c : b64str) {
        int v = ::AudioBabel::Utilities::base64UrlValue(c);
        values.push_back(static_cast<uint8_t>(v < 0 ? 0 : v));
    }
    return values;
}

auto IndexMetadata::extractMetadataFromIndex(const boost::multiprecision::cpp_int& index) -> IndexMetadata {
    // Derive the canonical bijective base-64 string for this index.
    std::string          b64str = ::AudioBabel::Utilities::indexToB64(index);
    std::vector<uint8_t> values = b64StringToValues(b64str);

    // Build metadata with content-derived labels
    IndexMetadata meta = buildMetadataFromBytesAndB64(values, b64str);

    // Calculate hierarchical position
    meta.position = calculateLibraryPosition(index);

    return meta;
}

// String overload: extract metadata directly from a bijective base-64 index string
auto IndexMetadata::extractMetadataFromIndex(const std::string& base64Index) -> IndexMetadata {
    // Validate that the string only uses the URL-safe alphabet.
    if (!::AudioBabel::Utilities::isValidBase64Url(base64Index)) {
        throw std::invalid_argument("Invalid base64 URL-safe string provided to extractMetadataFromIndex");
    }

    std::vector<uint8_t> values = b64StringToValues(base64Index);

    // Build metadata with content-derived labels
    IndexMetadata meta = buildMetadataFromBytesAndB64(values, base64Index);

    // Reconstruct the integer index from the bijective base-64 string for position.
    boost::multiprecision::cpp_int index = ::AudioBabel::Utilities::b64ToIndex(base64Index);
    meta.position                        = calculateLibraryPosition(index);

    return meta;
}

// Centralized helper that builds IndexMetadata from raw bytes and the corresponding base64 string.
// Define the static helper declared in the header so the symbol is available
auto IndexMetadata::buildMetadataFromBytesAndB64(const std::vector<uint8_t>& bytes, const std::string& b64str) -> IndexMetadata {
    IndexMetadata meta;
    if (b64str.empty()) {
        meta.genre  = "g0";
        meta.artist = "a0";
        meta.album  = "al0";
        meta.track  = "t0";
        // Generate cover even for empty index
        meta.cover = IndexMetadata::generateSvgCover(bytes, meta.track);
        return meta;
    }

    std::array<uint32_t, 4> weights = {0, 0, 0, 0};
    for (size_t i = 0; i < bytes.size(); ++i) {
        weights[i % 4] += static_cast<uint32_t>(bytes[i]);
    }
    uint32_t totalWeight = weights[0] + weights[1] + weights[2] + weights[3];
    if (totalWeight == 0) {
        weights     = {1, 1, 1, 1};
        totalWeight = 4;
    }

    size_t                b64Len = b64str.size();
    std::array<size_t, 4> lens   = {0, 0, 0, 0};

    // Closed-form weighted allocation with remainder distribution
    size_t sum = 0;
    for (int i = 0; i < 4; ++i) {
        lens[i] = (b64Len * weights[i]) / totalWeight;
        sum += lens[i];
    }

    // Distribute remaining characters round-robin to fields 0..(rem-1)
    size_t rem = b64Len - sum;
    for (size_t i = 0; i < rem; ++i) {
        lens[i % 4]++;
    }

    size_t pos = 0;
    meta.genre = (pos < b64Len) ? b64str.substr(pos, std::min(lens[0], b64Len - pos)) : "g";
    pos += lens[0];
    meta.artist = (pos < b64Len) ? b64str.substr(pos, std::min(lens[1], b64Len - pos)) : "a";
    pos += lens[1];
    meta.album = (pos < b64Len) ? b64str.substr(pos, std::min(lens[2], b64Len - pos)) : "al";
    pos += lens[2];
    meta.track = (pos < b64Len) ? b64str.substr(pos, std::min(lens[3], b64Len - pos)) : "t";

    std::string svg = IndexMetadata::generateSvgCover(bytes, meta.track);
    meta.cover      = svg;
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
