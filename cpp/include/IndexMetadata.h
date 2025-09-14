#ifndef INDEX_METADATA_H
#define INDEX_METADATA_H

#include <boost/multiprecision/cpp_int.hpp>
#include <string>
#include <vector>

namespace AudioBabel {

// Metadata type for audio indices (renamed from AudioMetaData).
class IndexMetadata {
   public:
    std::string          genre;
    std::string          artist;
    std::string          album;
    std::string          track;
    std::vector<uint8_t> cover; // optional small image bytes

    // Extract deterministic metadata from a big-integer index.
    static auto extractMetadataFromIndex(const boost::multiprecision::cpp_int& index) -> IndexMetadata;
    // Overload: extract metadata directly from a URL-safe base64 string
    // representation (no padding) of the index bytes.
    static auto extractMetadataFromIndex(const std::string& base64Index) -> IndexMetadata;
    // Public helper: generate a tiny SVG cover from bytes and a track string.
    // Returns the SVG markup as a std::string.
    static std::string generateSvgCover(const std::vector<uint8_t>& bytes, const std::string& track);
    // Validate whether a string contains only URL-safe base64 characters
    // (A-Z, a-z, 0-9, '-' and '_'). Empty string is considered valid.
    static bool isValidBase64(const std::string& s);
};

} // namespace AudioBabel

#endif // INDEX_METADATA_H
