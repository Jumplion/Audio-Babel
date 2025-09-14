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
};

} // namespace AudioBabel

#endif // INDEX_METADATA_H
