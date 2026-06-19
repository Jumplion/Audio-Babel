#ifndef AUDIO_INDEX_H
#define AUDIO_INDEX_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace AudioBabel {

/**
 * @file AudioIndex.h
 * @brief Public API for deterministic audio indexing and reconstruction.
 * 
 * @class AudioIndex
 * @brief Represents a deterministic audio index encoded as a big integer.
 * 
 * AudioIndex provides bidirectional conversion between audio data and cryptographically-large
 * indexes, enabling lossless reconstruction and hierarchical organization in the "Speaker of Babel"
 * library system. The index is a TRUE BIJECTION over the PCM sample payload only: every index
 * decodes to exactly one payload and every payload encodes to exactly one index, with no header,
 * version, or format metadata embedded.
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
 * zero samples produce different indices. No header is stored; a fixed default header
 * (PCM, 44100 Hz, 16-bit, mono) is applied only when writing a WAV on decode. The user-facing
 * index string is a bijective base-64 over the URL-safe alphabet (see Utilities::indexToB64).
 * There is intentionally NO integrity check: every alphabet-valid index decodes to a valid payload.
 *
 * @section usage Usage Example
 * @code
 * // Create index from WAV file
 * auto audioData = AudioIndex::extractAudioDataFromAudioFile("input.wav");
 * auto index = AudioIndex::audioDataToIndex(audioData);
 * 
 * // Reconstruct audio from index
 * auto reconstructed = AudioIndex::indexToAudioData(index);
 * FileWriters::exportAudioDataToWav(reconstructed, "output.wav");
 * 
 * // Extract metadata
 * auto metadata = IndexMetadata::extractMetadataFromIndex(index);
 * std::cout << "Genre: " << metadata.genre << std::endl;
 * @endcode
 *
 * @section thread_safety Thread Safety
 * All static methods are thread-safe.
 *
 * @see IndexMetadata for metadata extraction from indexes
 * @see LibraryPosition for hierarchical position calculation
 */
class AudioIndex {
   public:
    // ---------------------
    // Public data container
    // ---------------------

    /**
     * @struct AudioData
     * @brief Container for raw audio sample data and format parameters.
     * 
     * AudioData is the single source of truth for audio sample format and payload.
     * Fields are intentionally simple PODs (Plain Old Data) so tests and callers 
     * can construct or inspect them directly without accessor methods.
     * 
     * @note All multi-byte values in the samples vector are stored in little-endian
     *       byte order, matching WAV file format conventions.
     */
    struct AudioData {
        uint32_t             sample_rate;  ///< Sample rate in Hz (e.g., 44100)
        uint16_t             bit_rate;     ///< Bits per sample (8, 16, or 32 supported)
        uint16_t             num_channels; ///< Number of audio channels (1 = mono, 2 = stereo)
        uint16_t             audio_format; ///< Audio format code (1 = PCM)
        size_t               num_frames;   ///< Number of audio frames (samples per channel)
        std::vector<uint8_t> samples;      ///< PCM sample bytes (little-endian per-sample)
    };

    // ---------------------
    // Metadata
    // ---------------------
    // Metadata type was extracted to its own header `IndexMetadata.h`.
    // See cpp/include/IndexMetadata.h

    // ---------------------
    // Factory helpers
    // ---------------------

    /**
     * @brief Read a WAV file and extract audio data.
     * 
     * Parses a standard RIFF/WAVE PCM file and populates an AudioData structure.
     * The function expects a well-formed WAV file with a 'fmt ' chunk followed by
     * a 'data' chunk. Unknown chunks are skipped according to RIFF padding rules.
     * 
     * @param path File path to the .wav file
     * @return AudioData structure with parsed audio samples
     * @throws std::runtime_error if file cannot be opened, format is invalid,
     *         bit depth is unsupported, or file is truncated
     * 
     * @par Supported Formats
     * - PCM format only (audio_format = 1)
     * - Bit depths: 8, 16, or 32 bits per sample
     * - Any sample rate
     * - Any channel count
     * 
     * @note The returned AudioData.samples contains little-endian byte-order samples
     *       ready for direct use with FileWriters::exportAudioDataToWav().
     *
     * @see FileWriters::exportAudioDataToWav
     */
    static auto extractAudioDataFromAudioFile(const std::string& path) -> AudioData;

    /**
     * @brief Create AudioData from signed integer samples.
     * 
     * Converts a vector of host-order int32 values into an AudioData structure
     * with packed little-endian bytes per sample according to the specified bit depth.
     * 
     * @param samples PCM samples as signed 32-bit integers (mono interleaved)
     * @param sampleRate Sample rate in Hz (default: 44100)
     * @param bitDepth Bit depth in bits (default: 16; supported: 8, 16, 32)
     * @return AudioData structure ready for index encoding
     * 
     * @par Sample Packing
     * - 8-bit: Each int32 is packed as 1 byte (LSB only)
     * - 16-bit: Each int32 is packed as 2 bytes (little-endian)
     * - 32-bit: Each int32 is packed as 4 bytes (little-endian)
     * 
     * @note Assumes mono input (num_channels = 1)
     */
    static auto extractAudioDataFromSamples(const std::vector<int32_t>& samples, int sampleRate = 44100, int bitDepth = 16) -> AudioData;

    // ---------------------
    // Serialization / Deserialization
    // ---------------------

    /**
     * @brief Convert a PCM sample payload into a big integer index.
     *
     * Reads audioData.samples as little-endian 16-bit (B-ary) samples and applies the
     * bijective payload->integer mapping (digit = value + 1). The result depends ONLY on
     * the sample values; no header, version, or format metadata is embedded.
     *
     * @param audioData Audio data whose samples vector holds the PCM payload
     * @return Unique index as a boost::multiprecision::cpp_int
     *
     * @note Never throws on the payload: there is no validation that can reject an index.
     *
     * @par Performance
     * O(N) in the payload size: the integer is built with the closed-form identity n = V + S_L
     * (payload value plus the base-B repunit) using linear import_bits passes and a single
     * big-integer addition — not a per-sample bignum loop.
     *
     * @see indexToAudioData for the inverse operation
     */
    static auto audioDataToIndex(const AudioData& audioData) -> boost::multiprecision::cpp_int;

    /**
     * @brief Reconstruct a PCM sample payload from a big integer index.
     *
     * Applies the bijective integer->payload mapping, serializes each decoded 16-bit sample
     * little-endian into AudioData.samples, and applies the fixed default header
     * (PCM, 44100 Hz, 16-bit, mono) with num_frames = sampleCount / num_channels.
     *
     * @param index Big integer index produced by audioDataToIndex()
     * @return AudioData structure with reconstructed samples and default format parameters
     *
     * @note Does not throw on any alphabet-valid index; there is intentionally no integrity
     *       check. Trailing zero (silence) samples are preserved exactly.
     *
     * @par Performance
     * O(N) in the payload size: the sample count and digits are recovered from the same
     * n = V + S_L identity (magnitude check plus one subtraction), with no per-sample bignum
     * division.
     *
     * @see audioDataToIndex for the inverse operation
     */
    static auto indexToAudioData(const boost::multiprecision::cpp_int& index) -> AudioData;
};

} // namespace AudioBabel

#endif // AUDIO_INDEX_H
