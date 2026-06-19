#ifndef INDEX_H
#define INDEX_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <vector>

namespace AudioBabel {

/**
 * @file Index.h
 * @brief Public API for the deterministic PCM payload <-> big integer bijection.
 *
 * @class Index
 * @brief Bidirectional bijection between a raw PCM sample payload and a big integer.
 *
 * Index provides bidirectional conversion between a PCM sample payload and
 * cryptographically-large indexes, enabling lossless reconstruction and hierarchical
 * organization in the "Speaker of Babel" library system. The index is a TRUE BIJECTION
 * over the PCM sample payload only: every index decodes to exactly one payload and every
 * payload encodes to exactly one index, with no header, version, or format metadata
 * embedded. Index knows nothing about WAV headers, sample rates, or files — see FileIO
 * for reading/writing PCM payloads to/from WAV files.
 *
 * @section index_format Index Format (payload-only bijection)
 * The index encodes ONLY the PCM sample payload. The atomic unit is one PCM sample, interpreted
 * as an UNSIGNED little-endian value in 0..B-1 where B = 1u << DEFAULT_BIT_DEPTH (65536 at the
 * 16-bit default). The integer is built with bijective numeration (digit = value + 1):
 *
 * @par Encoding (samples -> integer):
 * - n = 0; for each sample v in order: n = n * B + (v + 1)
 *
 * @par Decoding (integer -> samples):
 * - while n > 0: { n -= 1; v = n mod B; emit v; n = n / B } then reverse
 *
 * Because the digit is value+1, trailing zero (silence) samples are preserved: k vs k+1 trailing
 * zero samples produce different indices. The user-facing index string is a bijective base-64
 * over the URL-safe alphabet (see Utilities::indexToB64). There is intentionally NO integrity
 * check: every alphabet-valid index decodes to a valid payload.
 *
 * @section usage Usage Example
 * @code
 * // Create index from WAV file
 * auto audioData = FileIO::readWav("input.wav");
 * auto index = Index::encode(audioData.samples);
 *
 * // Reconstruct audio from index
 * auto samples = Index::decode(index);
 * FileIO::writeWav(samples, "output.wav");
 *
 * // Extract metadata
 * auto metadata = IndexMetadata::extractMetadataFromIndex(index);
 * std::cout << "Genre: " << metadata.genre << std::endl;
 * @endcode
 *
 * @section thread_safety Thread Safety
 * All static methods are thread-safe.
 *
 * @see FileIO for reading/writing PCM payloads to/from WAV files
 * @see IndexMetadata for metadata extraction from indexes
 * @see LibraryPosition for hierarchical position calculation
 */
class Index {
   public:
    /**
     * @brief Convert a PCM sample payload into a big integer index.
     *
     * Reads samples as little-endian 16-bit (B-ary) values and applies the bijective
     * payload->integer mapping (digit = value + 1). The result depends ONLY on the
     * sample values; no header, version, or format metadata is embedded.
     *
     * @param samples PCM sample payload bytes (little-endian per-sample)
     * @return Unique index as a boost::multiprecision::cpp_int
     *
     * @note Never throws on the payload: there is no validation that can reject an index.
     *
     * @par Performance
     * O(N) in the payload size: the integer is built with the closed-form identity n = V + S_L
     * (payload value plus the base-B repunit) using linear import_bits passes and a single
     * big-integer addition — not a per-sample bignum loop.
     *
     * @see decode for the inverse operation
     */
    static auto encode(const std::vector<uint8_t>& samples) -> boost::multiprecision::cpp_int;

    /**
     * @brief Reconstruct a PCM sample payload from a big integer index.
     *
     * Applies the bijective integer->payload mapping and serializes each decoded 16-bit
     * sample little-endian into the returned byte vector. No header or format metadata is
     * attached — see FileIO for wrapping the payload with a WAV header.
     *
     * @param index Big integer index produced by encode()
     * @return Reconstructed PCM sample payload bytes (little-endian per-sample)
     *
     * @note Does not throw on any alphabet-valid index; there is intentionally no integrity
     *       check. Trailing zero (silence) samples are preserved exactly.
     *
     * @par Performance
     * O(N) in the payload size: the sample count and digits are recovered from the same
     * n = V + S_L identity (magnitude check plus one subtraction), with no per-sample bignum
     * division.
     *
     * @see encode for the inverse operation
     */
    static auto decode(const boost::multiprecision::cpp_int& index) -> std::vector<uint8_t>;
};

} // namespace AudioBabel

#endif // INDEX_H
