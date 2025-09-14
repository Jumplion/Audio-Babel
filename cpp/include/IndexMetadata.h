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

    // Forward declaration for helper used by both overloads
    static auto buildMetadataFromBytesAndB64(const std::vector<uint8_t>& bytes, const std::string& b64str) -> IndexMetadata;

    // Extract deterministic metadata from a big-integer index.
    static auto extractMetadataFromIndex(const boost::multiprecision::cpp_int& index) -> IndexMetadata;

    // Overload: extract metadata directly from a URL-safe base64 string
    // representation (no padding) of the index bytes.
    static auto extractMetadataFromIndex(const std::string& base64Index) -> IndexMetadata;

    // Generate a tiny SVG cover from bytes and a track string.
    // Returns the SVG markup as a std::string.
    static auto generateSvgCover(const std::vector<uint8_t>& bytes, const std::string& track) -> std::string;
};

} // namespace AudioBabel

#endif // INDEX_METADATA_H
