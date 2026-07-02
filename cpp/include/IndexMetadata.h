#ifndef INDEX_METADATA_H
#define INDEX_METADATA_H

#include <boost/multiprecision/cpp_int.hpp>
#include <string>
#include <vector>

#include "IndexNaming.h"
#include "LibraryPosition.h"

namespace AudioBabel {

// Metadata extracted deterministically from an audio index: content labels
// (genre/artist/album/track, via IndexNaming), cover art (an SVG mosaic
// generated from Feistel-permuted index bytes, its own key), and hierarchical
// LibraryPosition. See IndexNaming.h and LibraryPosition.h for the respective
// algorithms.
class IndexMetadata {
   public:
    std::string genre;
    std::string artist;
    std::string album;
    std::string track;
    std::string cover; // 256x256 SVG markup

    LibraryPosition position;

    // Computes LibraryPosition and the four IndexNaming labels for `index`,
    // then generates the cover art.
    static auto extractMetadataFromIndex(const boost::multiprecision::cpp_int& index) -> IndexMetadata;

    // Same as the cpp_int overload, from a URL-safe base64 (no padding) index
    // string. Throws std::invalid_argument on invalid characters.
    static auto extractMetadataFromIndex(const std::string& base64Index) -> IndexMetadata;

    // Side length, in canvas pixels, of one mosaic tile in the production
    // cover. The fixed 256x256 canvas is tiled with (256/DEFAULT_CELL_SIZE)^2
    // = 64x64 = 4096 such tiles.
    static constexpr unsigned DEFAULT_CELL_SIZE = 4;

    // Exact byte count a mosaic of the given tile size reads (3 bytes/cell, no
    // padding) — what a caller must supply to generateSvgCover to avoid any
    // cell falling back to black. Smaller cellSize means more, smaller tiles,
    // so it needs more bytes.
    static constexpr auto pixelBytesNeeded(unsigned cellSize) -> size_t {
        size_t cellsPerSide = cellSize > 0 ? CANVAS_SIZE / cellSize : 1;
        return cellsPerSide * cellsPerSide * 3;
    }

    // Side length, in pixels, of the square SVG canvas generateSvgCover draws.
    static constexpr unsigned CANVAS_SIZE = 256;

    // Builds a native-resolution (256/cellSize)^2 truecolor bitmap directly
    // from `bytes` (MSB-first; each tile takes the next three bytes,
    // reading order, as its R/G/B — a direct, bijective dump, no
    // hashing/PRNG, so a target image's quantized bytes decode back to the
    // same image), embeds it as a single raster <image> in a 256x256 SVG
    // (image-rendering: pixelated so tile edges stay crisp), and overlays
    // centered white text with the track identifier over a translucent
    // backdrop. A tile whose bytes run past the end of `bytes` renders black
    // rather than throwing. The bitmap is an uncompressed 24-bit BMP (no
    // color table, no compression) so it can be encoded without a
    // compression library while staying lossless.
    //
    // cellSize (default DEFAULT_CELL_SIZE) is exposed mainly for
    // benchmarks/tests to sweep tile sizes; production code should use the
    // default.
    static auto generateSvgCover(const std::vector<uint8_t>& bytes, const std::string& track, unsigned cellSize = DEFAULT_CELL_SIZE) -> std::string;

    // Inverse of the cover pipeline in extractMetadataFromIndex: walks target
    // `pixels` back through the inverse permutation to recover cover
    // material, then lifts it to full-size indexes by prepending random
    // high "discriminator" bits (same construction as
    // IndexNaming::constructIndexesForNames). Every returned index decodes
    // to exactly the requested cover (names/position/audio vary per
    // candidate).
    //
    // `pixels` must be exactly pixelBytesNeeded(DEFAULT_CELL_SIZE) bytes of
    // packed 8-bit RGB in reading order; throws std::invalid_argument
    // otherwise. `seed` makes a given (pixels, seed) pair reproducible.
    //
    // The cover material domain is [0, 2^coverBits - 3), so the top three
    // pixel patterns (all-white and its two nearest neighbours) are
    // unreachable and get clamped to the nearest representable pattern — a
    // difference of at most 3 in one tile's blue channel, invisible in
    // practice.
    static auto constructIndexesForCover(const std::vector<uint8_t>& pixels,
                                         size_t                      count,
                                         uint64_t                    seed) -> std::vector<boost::multiprecision::cpp_int>;

   private:
    // Assembles an IndexMetadata from precomputed parts: copies `names` in
    // and generates the SVG cover from `bytes` and the track name.
    static auto buildMetadataFromBytesAndPosition(const std::vector<uint8_t>& bytes,
                                                  const LibraryPosition&      position,
                                                  const IndexNaming::Names&   names) -> IndexMetadata;
};

} // namespace AudioBabel

#endif // INDEX_METADATA_H
