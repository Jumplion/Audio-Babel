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
 * library system. Each index deterministically encodes audio samples along with format metadata
 * (sample rate, bit depth, channels) in a single big integer.
 * 
 * @section index_format Index Format
 * Each index consists of a 13-byte header followed by PCM sample data, all packed into
 * a big integer using MSB-first byte order:
 * 
 * @par Header Structure (13 bytes):
 * - Byte 0: Version (0x01)
 * - Bytes 1-4: Number of frames (uint32_t, little-endian)
 * - Bytes 5-8: Sample rate in Hz (uint32_t, little-endian)
 * - Bytes 9-10: Bit depth (uint16_t, little-endian)
 * - Bytes 11-12: Number of channels (uint16_t, little-endian)
 * - Bytes 13+: PCM sample data (little-endian per-sample format)
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
     * @brief Convert audio data and metadata into a big integer index.
     * 
     * Serializes audio data with a 13-byte header into a single big integer.
     * The header encodes version, frame count, sample rate, bit depth, and channel count,
     * enabling lossless reconstruction of the exact audio parameters.
     * 
     * @param audioData Audio data structure to encode
     * @return Unique index as a boost::multiprecision::cpp_int
     * @throws std::runtime_error if bit depth is not supported (must be 8, 16, or 32)
     * 
     * @par Index Structure
     * The index is constructed by concatenating:
     * 1. A 13-byte header (version + audio parameters)
     * 2. PCM sample data (already in little-endian format)
     * Then converting the entire byte array to a big integer (MSB-first).
     * 
     * @par Performance
     * Typical conversion time for 2 minutes of 44.1kHz/16-bit audio is ~10-50ms
     * depending on CPU. Use getLastDebugInfo() to retrieve timing information.
     * 
     * @see indexToAudioData for the inverse operation
     * @see getLastDebugInfo for performance diagnostics
     */
    static auto audioDataToIndex(const AudioData& audioData) -> boost::multiprecision::cpp_int;

    /**
     * @brief Reconstruct audio data from a big integer index.
     * 
     * Deserializes a big integer index back into AudioData by extracting the 13-byte
     * header and PCM samples. The function handles indexes that may have leading zero
     * bytes omitted by export_bits and reconstructs the full audio payload.
     * 
     * @param index Big integer index produced by audioDataToIndex()
     * @return AudioData structure with reconstructed samples and parameters
     * @throws std::runtime_error if header is malformed, version is unsupported,
     *         or calculated payload size is inconsistent
     * 
     * @par Robustness
     * The function validates:
     * - Header version (must be 0x01)
     * - Bit depth (must be 8, 16, or 32)
     * - Payload size consistency (num_frames × bytes_per_sample)
     * 
     * @note Leading zero bytes in the PCM payload are preserved during reconstruction
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
        size_t   import_pcm_bytes      = 0; ///< Bytes fed to import_bits in audioDataToIndex
        size_t   import_expected_bytes = 0; ///< Expected byte count for import
        size_t   export_pcm_bytes      = 0; ///< Bytes returned by export_bits in indexToAudioData
        size_t   export_expected_bytes = 0; ///< Expected byte count for export
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
