#ifndef AUDIO_INDEX_H
#define AUDIO_INDEX_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "IndexMetadata.h"

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
 * AudioIndex::exportAudioDataToWav(reconstructed, "output.wav");
 * 
 * // Extract metadata
 * auto metadata = AudioIndex::indexToMetadata(index);
 * std::cout << "Genre: " << metadata.genre << std::endl;
 * @endcode
 * 
 * @section thread_safety Thread Safety
 * Static methods are thread-safe except for debug info getters/setters which use
 * a shared static variable. Instance methods require external synchronization.
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
    // Construction / lifecycle
    // ---------------------

    /**
     * @brief Copy assignment operator.
     * @param other AudioIndex to copy from
     * @return Reference to this object
     */
    auto operator=(const AudioIndex& other) -> AudioIndex&;

    /**
     * @brief Equality comparison operator.
     * @param other AudioIndex to compare with
     * @return true if audio data matches exactly, false otherwise
     */
    auto operator==(const AudioIndex& other) const -> bool;

    /**
     * @brief Inequality comparison operator.
     * @param other AudioIndex to compare with
     * @return true if audio data differs, false otherwise
     */
    auto operator!=(const AudioIndex& other) const -> bool;

    // ---------------------
    // Factory helpers
    // ---------------------

    /**
     * @brief Create an AudioIndex from raw PCM samples.
     * 
     * This factory function deterministically computes an index fingerprint and
     * extracts hierarchical position codes from the provided audio samples.
     * 
     * @param samples PCM samples as signed 32-bit integers (mono interleaved)
     * @param sampleRate Sample rate in Hz (default: 44100)
     * @param bitDepth Bit depth in bits (default: 16; supported: 8, 16, 32)
     * @return AudioIndex instance with computed metadata
     * @throws std::runtime_error if bitDepth is not supported
     * 
     * @par Example
     * @code
     * std::vector<int32_t> samples = {0, 1000, -1000, 2000};
     * auto index = AudioIndex::fromAudioSamples(samples, 44100, 16);
     * @endcode
     */
    static auto fromAudioSamples(const std::vector<int32_t>& samples, int sampleRate = 44100, int bitDepth = 16) -> AudioIndex;

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
     *       ready for direct use with exportAudioDataToWav().
     * 
     * @see exportAudioDataToWav
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
     * @see getLastDebugInfo for performance diagnostics
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

    // ---------------------
    // Debug / diagnostics
    // ---------------------

    /**
     * @struct DebugInfo
     * @brief Performance and diagnostic information for index operations.
     * 
     * This structure captures timing and byte-count metrics from the most recent
     * audioDataToIndex() or indexToAudioData() call. Useful for profiling and
     * debugging serialization issues.
     * 
     * @note Thread-local storage; each thread maintains its own debug info.
     */
    struct DebugInfo {
        size_t   import_pcm_bytes      = 0; ///< Payload bytes consumed by audioDataToIndex
        size_t   import_expected_bytes = 0; ///< Expected payload byte count for encode
        size_t   export_pcm_bytes      = 0; ///< Payload bytes produced by indexToAudioData
        size_t   export_expected_bytes = 0; ///< Expected payload byte count for decode
        uint64_t audioDataToIndexMs    = 0; ///< Milliseconds spent in audioDataToIndex
        uint64_t indexToAudioDataMs    = 0; ///< Milliseconds spent in indexToAudioData
    };

    /**
     * @brief Retrieve debug information from the last serialization operation.
     * 
     * Returns performance metrics and byte counts from the most recent call to
     * audioDataToIndex() or indexToAudioData() in the current thread.
     * 
     * @return DebugInfo structure with timing and diagnostic data
     * 
     * @par Thread Safety
     * This function accesses thread-local storage and is safe to call concurrently.
     * 
     * @see clearLastDebugInfo
     */
    static auto getLastDebugInfo() -> DebugInfo;

    /**
     * @brief Clear the cached debug information.
     * 
     * Resets the debug info structure to default values. Useful when you want to
     * ensure fresh metrics for subsequent operations.
     * 
     * @see getLastDebugInfo
     */
    static void clearLastDebugInfo();

    // ---------------------
    // File I/O helpers
    // ---------------------

    /**
     * @brief Export audio data to a WAV file.
     * 
     * Writes a standard RIFF/WAVE PCM file with the provided audio data.
     * The output file will be a valid WAV file that can be opened by any
     * standard audio player or editor.
     * 
     * @param audioData Audio data structure to write
     * @param path Output file path (will be created or overwritten)
     * @throws std::runtime_error if file cannot be opened for writing
     * 
     * @note Compatibility alias — delegates to FileWriters::exportAudioDataToWav.
     *       Prefer calling FileWriters::exportAudioDataToWav directly in new code.
     * 
     * @see extractAudioDataFromAudioFile for the inverse operation
     */
    [[deprecated("Use FileWriters::exportAudioDataToWav directly")]]
    static void exportAudioDataToWav(const AudioData& audioData, const std::string& path);

    /**
     * @brief Write an index to a text file in URL-safe base64 encoding.
     * 
     * Serializes a big integer index to a base64-encoded text file without padding.
     * The filename is automatically generated from the index's first bytes unless
     * explicitly provided.
     * 
     * @param index Big integer index to write
     * @param outDir Output directory (default: cpp/tests/indexes/)
     * @param filename Base filename without extension (default: auto-generated from index bytes)
     * 
     * @par File Format
     * The output file contains a single line of URL-safe base64 text (alphabet: A-Za-z0-9-_)
     * with no padding characters, followed by a newline.
     * 
     * @note Compatibility alias — delegates to FileWriters::writeIndexToFile.
     *       Prefer calling FileWriters::writeIndexToFile directly in new code.
     */
    [[deprecated("Use FileWriters::writeIndexToFile directly")]]
    static void writeIndexToFile(const boost::multiprecision::cpp_int& index,
                                 const std::string&                    outDir   = std::string(),
                                 const std::string&                    filename = std::string());

    // ---------------------
    // Metadata helpers
    // ---------------------

    /**
     * @brief Extract metadata from a big integer index.
     * 
     * Derives hierarchical library position and generates album cover art
     * from the index's byte representation. The metadata includes genre,
     * artist, album, and track identifiers computed deterministically.
     * 
     * @param index Big integer index to extract metadata from
     * @return IndexMetadata structure with position codes and cover art
     * 
     * @par Metadata Fields
     * - genre, artist, album, track: URL-safe base64 strings of varying lengths
     * - coverData: 256×256 SVG with color derived from index bytes
     * 
     * @see IndexMetadata for details on metadata structure
     */
    static auto indexToMetadata(const boost::multiprecision::cpp_int& index) -> IndexMetadata;

    /**
     * Get the sample rate of the audio index.
     * @returns Sample rate in Hz
     */
    [[nodiscard]] auto getSampleRate() const -> int {
        return static_cast<int>(audioData.sample_rate);
    }

    /**
     * Get the duration of the audio index.
     * @returns Duration in seconds
     */
    [[nodiscard]] auto getDuration() const -> double {
        return (audioData.sample_rate > 0) ? (static_cast<double>(audioData.num_frames) / static_cast<double>(audioData.sample_rate)) : 0.0;
    }

    /**
     * Get the bit depth of the audio index.
     * @returns Bit depth in bits
     */
    [[nodiscard]] auto getBitDepth() const -> int {
        return static_cast<int>(audioData.bit_rate);
    }

    /**
     * Get the metadata computed for this index by fromAudioSamples().
     * @returns IndexMetadata populated during construction (default-constructed if unavailable)
     */
    [[nodiscard]] auto getMetadata() const -> const IndexMetadata& {
        return metadata;
    }

   private:
    AudioData     audioData;
    IndexMetadata metadata;
};

} // namespace AudioBabel

#endif // AUDIO_INDEX_H
