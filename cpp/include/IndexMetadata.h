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
 * - **Cover Art**: SVG pixel-mosaic image generated from index bytes
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
     * 1. Export index to bytes (MSB-first); the leading pixelBytesNeeded(DEFAULT_CELL_SIZE)
     *    of them become the cover mosaic's pixels directly
     * 2. Compute LibraryPosition from the index (for the Browse hierarchy)
     * 3. Derive genre/artist/album/track from the index via IndexNaming::namesForIndex
     * 4. Generate SVG cover from the leading bytes and the track name
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

    /// Side length, in canvas pixels, of one mosaic tile in the production
    /// cover. The fixed 256x256 canvas (see generateSvgCover) is tiled with
    /// (256 / DEFAULT_CELL_SIZE)^2 = 16x16 = 256 such tiles.
    static constexpr unsigned DEFAULT_CELL_SIZE = 16;

    /// Exact byte count a mosaic of the given tile size reads (3 bytes/cell,
    /// no padding) -- the size a caller must supply to `generateSvgCover` to
    /// avoid any cell falling back to black. Smaller cellSize means more,
    /// smaller tiles (finer image), so it needs *more* bytes: at cellSize=1
    /// every one of the canvas's 256*256 pixels is its own tile.
    static constexpr auto pixelBytesNeeded(unsigned cellSize) -> size_t {
        size_t cellsPerSide = cellSize > 0 ? CANVAS_SIZE / cellSize : 1;
        return cellsPerSide * cellsPerSide * 3;
    }

    /// Side length, in pixels, of the square SVG canvas generateSvgCover draws.
    static constexpr unsigned CANVAS_SIZE = 256;

    /**
     * @brief Generate an SVG album cover directly from index bytes.
     *
     * Creates a 256×256 SVG image tiled with a mosaic of cellSize×cellSize
     * pixel tiles, each individually colored, plus centered white text over
     * a translucent backdrop displaying the track identifier.
     *
     * @param bytes Index bytes (MSB-first). Read three bytes at a time, in
     *   reading order (left-to-right, top-to-bottom), as each tile's (R, G, B)
     *   -- a direct, literal dump of bytes into pixels. A tile whose bytes run
     *   past the end of `bytes` renders black (0, 0, 0) rather than throwing.
     * @param track Track identifier string to display
     * @param cellSize Side length, in canvas pixels, of one mosaic tile
     *   (default DEFAULT_CELL_SIZE). Smaller values mean *more*, finer tiles
     *   -- cellSize=1 is one tile per canvas pixel (a true bitmap); cellSize=256
     *   is a single tile covering the whole canvas (a flat fill). Exposed
     *   mainly so benchmarks/tests can sweep tile sizes; production code
     *   should use the default.
     * @return SVG markup as a string
     *
     * @par Algorithm
     * There is no hashing or PRNG step: tile (row, col), visited in reading
     * order, takes the next three bytes off `bytes` as its red, green, and
     * blue channel, full stop. That makes the mapping from bytes to pixels
     * bijective in both directions -- given a target image, quantize it down
     * to (256/cellSize) x (256/cellSize) 8-bit-RGB tiles and write those bytes
     * at the same offset an index's cover reads from, and decoding that index
     * reproduces the image exactly. (The prior version seeded a splitmix64
     * PRNG stream from the bytes instead; PRNG output is one-way, so
     * reproducing a target image would have meant searching a seed space
     * profoundly smaller than the space of possible images -- not a search
     * anyone could ever finish.)
     *
     * @par SVG Structure
     * - Viewbox: 0 0 256 256
     * - Mosaic: (256/cellSize)^2 tiles, each filled directly from three bytes
     *   of `bytes`
     * - Text: Track string centered in white, 20px font, over a translucent
     *   dark backdrop for legibility against arbitrary cell colors
     */
    static auto generateSvgCover(const std::vector<uint8_t>& bytes, const std::string& track, unsigned cellSize = DEFAULT_CELL_SIZE)
        -> std::string;

   private:
    /**
     * @brief Internal helper to assemble metadata from precomputed parts.
     *
     * Copies the already-derived genre/artist/album/track `names` (see
     * IndexNaming::namesForIndex) into the result and generates the SVG cover
     * from `bytes` and the track name.
     *
     * @param bytes Index bytes (MSB-first), used directly as the cover art mosaic's pixels
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
