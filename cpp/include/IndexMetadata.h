#ifndef INDEX_METADATA_H
#define INDEX_METADATA_H

#include <boost/multiprecision/cpp_int.hpp>
#include <string>
#include <vector>

#include "LibraryPosition.h"

namespace AudioBabel {

/**
 * @class IndexMetadata
 * @brief Metadata extracted deterministically from audio indexes.
 * 
 * IndexMetadata represents hierarchical library position and content-derived labels
 * for organizing audio in the "Speaker of Babel" system. All fields are computed
 * deterministically from the index's byte representation, ensuring consistency across
 * serialization and deserialization.
 * 
 * @par Metadata Components
 * - **Content Labels**: genre, artist, album, track (URL-safe base64 strings)
 * - **Cover Art**: SVG image generated from index bytes
 * - **Position**: Hierarchical location (room, wall, shelf, track) in the library
 * 
 * @see LibraryPosition for hierarchical position calculation
 */
class IndexMetadata {
   public:
    std::string genre;  ///< Genre identifier (URL-safe base64, length weighted by byte sums)
    std::string artist; ///< Artist identifier (URL-safe base64, length weighted by byte sums)
    std::string album;  ///< Album identifier (URL-safe base64, length weighted by byte sums)
    std::string track;  ///< Track identifier (URL-safe base64, length weighted by byte sums)
    std::string cover;  ///< Album cover art (256×256 SVG markup as string)

    LibraryPosition position; ///< Hierarchical position in the library (room/wall/shelf/track)

    /**
     * @brief Extract metadata from a big integer index.
     * 
     * Derives hierarchical position and content labels from the index's byte
     * representation. The function converts the index to bytes (MSB-first) and
     * URL-safe base64, then applies weighted length distribution to create
     * genre, artist, album, and track strings.
     * 
     * @param index Big integer index to extract metadata from
     * @return IndexMetadata structure with all fields populated
     * 
     * @par Algorithm
     * 1. Export index to bytes (MSB-first)
     * 2. Encode bytes as URL-safe base64 (no padding)
     * 3. Compute LibraryPosition from bytes
     * 4. Split base64 string into four fields by weighted lengths
     * 5. Generate SVG cover from first bytes and track string
     * 
     * @see extractMetadataFromIndex(const std::string&) for base64 overload
     */
    static auto extractMetadataFromIndex(const boost::multiprecision::cpp_int& index) -> IndexMetadata;

    /**
     * @brief Extract metadata from a URL-safe base64 string representation.
     * 
     * Decodes a URL-safe base64 string (no padding) back to bytes and derives
     * metadata using the same algorithm as the big integer overload. This is more
     * efficient when the base64 representation is already available.
     * 
     * @param base64Index URL-safe base64 string (alphabet: A-Za-z0-9-_, no padding)
     * @return IndexMetadata structure with all fields populated
     * @throws std::invalid_argument if base64Index contains invalid characters
     * 
     * @par Input Requirements
     * - Must use URL-safe base64 alphabet (A-Za-z0-9-_)
     * - No padding characters (=) allowed
     * - Empty string yields default metadata (g0, a0, al0, t0)
     * 
     * @see extractMetadataFromIndex(const boost::multiprecision::cpp_int&) for index overload
     */
    static auto extractMetadataFromIndex(const std::string& base64Index) -> IndexMetadata;

    /**
     * @brief Generate an SVG album cover from index bytes.
     * 
     * Creates a 256×256 SVG image with a solid background color derived from
     * the first three bytes of the index, and centered white text displaying
     * the track identifier.
     * 
     * @param bytes Index bytes (MSB-first)
     * @param track Track identifier string to display
     * @return SVG markup as a string
     * 
     * @par SVG Structure
     * - Viewbox: 0 0 256 256
     * - Background: Solid fill color from RGB(bytes[0], bytes[1], bytes[2])
     * - Text: Track string centered in white, 20px font
     * 
     * @note If bytes contains fewer than 3 elements, missing bytes default to 0
     */
    static auto generateSvgCover(const std::vector<uint8_t>& bytes, const std::string& track) -> std::string;

   private:
    /**
     * @brief Internal helper to build metadata from bytes and base64 string.
     * 
     * Splits the base64 string into four weighted-length substrings (genre, artist,
     * album, track) based on byte sums. This ensures deterministic field lengths
     * that vary with index content.
     * 
     * @param bytes Index bytes (MSB-first)
     * @param b64str URL-safe base64 representation of bytes
     * @return IndexMetadata with all fields populated
     * 
     * @note This is an internal implementation detail shared by both public overloads
     */
    static auto buildMetadataFromBytesAndB64(const std::vector<uint8_t>& bytes, const std::string& b64str) -> IndexMetadata;
};

} // namespace AudioBabel

#endif // INDEX_METADATA_H
