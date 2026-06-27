#ifndef INDEX_METADATA_H
#define INDEX_METADATA_H

#include <boost/multiprecision/cpp_int.hpp>
#include <string>
#include <vector>

#include "IndexNaming.h"
#include "LibraryPosition.h"

namespace AudioBabel {

/**
 * @class IndexMetadata
 * @brief Metadata extracted deterministically from audio indexes.
 *
 * IndexMetadata represents hierarchical library position and content-derived labels
 * for organizing audio in the "Speaker of Babel" system.
 *
 * @par Metadata Components
 * - **Content Labels**: genre, artist, album, track — four deterministic names
 *   derived from the whole index via a keyed, invertible byte permutation, so
 *   neighbouring indexes get wildly different names and any desired names can be
 *   inverted back into indexes that carry them. See IndexNaming for the
 *   algorithm.
 * - **Cover Art**: SVG image generated from index bytes
 * - **Position**: Hierarchical location (room, wall, shelf, track) in the library
 *
 * @see LibraryPosition for hierarchical position calculation
 * @see IndexNaming for the cosmetic name generation algorithm
 */
class IndexMetadata {
   public:
    std::string genre;  ///< Genre name, derived from the whole index — see IndexNaming::namesForIndex
    std::string artist; ///< Artist name, derived from the whole index — see IndexNaming::namesForIndex
    std::string album;  ///< Album name, derived from the whole index — see IndexNaming::namesForIndex
    std::string track;  ///< Track name, derived from the whole index — see IndexNaming::namesForIndex
    std::string cover;  ///< Album cover art (256×256 SVG markup as string)

    LibraryPosition position; ///< Hierarchical position in the library (room/wall/shelf/track)

    /**
     * @brief Extract metadata from a big integer index.
     *
     * Computes the index's LibraryPosition (for navigation) and derives the four
     * genre/artist/album/track names from the index itself via IndexNaming.
     *
     * @param index Big integer index to extract metadata from
     * @return IndexMetadata structure with all fields populated
     *
     * @par Algorithm
     * 1. Export index to bytes (MSB-first), for cover art color only
     * 2. Compute LibraryPosition from the index (for the Browse hierarchy)
     * 3. Derive genre/artist/album/track from the index via IndexNaming::namesForIndex
     * 4. Generate SVG cover from the first bytes and the track name
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
     * - Empty string is index/room 0 — gets a normally generated name like any other index
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
     * @brief Internal helper to assemble metadata from precomputed parts.
     *
     * Copies the already-derived genre/artist/album/track `names` (see
     * IndexNaming::namesForIndex) into the result and generates the SVG cover
     * from `bytes` and the track name.
     *
     * @param bytes Index bytes (MSB-first), used only for cover art color
     * @param position Already-computed LibraryPosition for this index
     * @param names Already-derived metadata names for this index
     * @return IndexMetadata with all fields populated
     *
     * @note This is an internal implementation detail shared by both public overloads
     */
    static auto buildMetadataFromBytesAndPosition(const std::vector<uint8_t>& bytes,
                                                  const LibraryPosition&      position,
                                                  const IndexNaming::Names&   names) -> IndexMetadata;
};

} // namespace AudioBabel

#endif // INDEX_METADATA_H
