#ifndef AUDIO_INDEX_H
#define AUDIO_INDEX_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "IndexMetadata.h"

namespace AudioBabel {

/*
 * AudioIndex.h
 * ------------
 * Public API for the AudioIndex type. The class represents a deterministic
 * index encoded using GMP's big integers used for reconstruction.
 */

class AudioIndex {
   public:
    // ---------------------
    // Public data container
    // ---------------------
    // AudioData is the single source of truth for sample format and payload.
    // Fields are intentionally simple PODs so tests and callers can construct
    // or inspect them directly.
    struct AudioData {
        uint32_t             sample_rate;  // Sample rate in Hz (e.g. 44100)
        uint16_t             bit_rate;     // Bits per sample (8, 16 or 32)
        uint16_t             num_channels; // Number of audio channels (1 = mono, 2 = stereo)
        uint16_t             audio_format; // Audio format code (1 = PCM)
        size_t               num_frames;   // Number of audio frames (per-channel)
        std::vector<uint8_t> samples;      // PCM bytes (little-endian per-sample)
    };

    // ---------------------
    // Metadata
    // ---------------------
    // Metadata type was extracted to its own header `IndexMetadata.h`.
    // See cpp/include/IndexMetadata.h

    // ---------------------
    // Construction / lifecycle
    // ---------------------
    auto operator=(const AudioIndex& other) -> AudioIndex&;
    auto operator==(const AudioIndex& other) const -> bool;
    auto operator!=(const AudioIndex& other) const -> bool;

    // ---------------------
    // Factory helpers
    // ---------------------

    /**
     * Create an AudioIndex from raw PCM samples. This deterministically
     * computes a fingerprint and extracts hierarchical mpz codes.
     * @param samples PCM samples (mono interleaved)
     * @param sampleRate sample rate in Hz
     * @param bitDepth bit depth (typically 16 or 32)
     * @returns AudioIndex instance
     */
    static auto fromAudioSamples(const std::vector<int32_t>& samples, int sampleRate = 44100, int bitDepth = 16) -> AudioIndex;

    /**
     * Read a WAV file into an AudioData structure. The implementation expects
     * a standard RIFF/WAVE PCM file and populates sample bytes as little-endian
     * per-sample values (ready for exportAudioDataToWav).
     * @param path File path to the .wav file
     * @returns AudioData instance
     */
    static auto extractAudioDataFromAudioFile(const std::string& path) -> AudioData;

    /**
     * Create AudioData from a vector of signed integer samples (host int32
     * values). The returned AudioData.samples contains packed little-endian
     * bytes per sample according to bitDepth.
     * @param samples PCM samples (mono interleaved)
     * @param sampleRate Sample rate in Hz
     * @param bitDepth Bit depth (typically 16 or 32)
     * @returns AudioData instance
     */
    static auto extractAudioDataFromSamples(const std::vector<int32_t>& samples, int sampleRate = 44100, int bitDepth = 16) -> AudioData;

    // ---------------------
    // Serialization / Deserialization
    // ---------------------

    /**
     *  Convert audio payload + header into a single big integer index. The
     *  implementation packs a fixed 16-byte big-endian header into the least
     *  significant bytes and places the PCM payload as the more significant
     * bytes (MSB-first) so import_bits/export_bits can be used efficiently.
     * @param audioData Audio data to convert
     * @returns Unique index as a big integer
     */
    static auto audioDataToIndex(const AudioData& audioData) -> boost::multiprecision::cpp_int;

    /**
     * Reconstruct AudioData from an index produced by audioDataToIndex. The
     * function uses export_bits and will pad or truncate MSB-side bytes to
     * match the expected PCM payload length when export_bits omits leading
     * zero bytes.
     * @param index Unique index to convert
     * @returns Audio data reconstructed from the index
     */
    static auto indexToAudioData(const boost::multiprecision::cpp_int& index) -> AudioData;

    // ---------------------
    // Debug / diagnostics
    // ---------------------
    struct DebugInfo {
        size_t   import_pcm_bytes      = 0; // bytes fed to import_bits in audioDataToIndex
        size_t   import_expected_bytes = 0;
        size_t   export_pcm_bytes      = 0; // bytes returned by export_bits in indexToAudioData
        size_t   export_expected_bytes = 0;
        uint64_t audioDataToIndexMs    = 0; // ms spent in audioDataToIndex
        uint64_t indexToAudioDataMs    = 0; // ms spent in indexToAudioData
    };

    // Retrieve the last debug info populated by the most recent serialization
    // or deserialization call on this translation unit.
    static auto getLastDebugInfo() -> DebugInfo;
    static void clearLastDebugInfo();

    // File I/O helpers were moved to FileWriters to separate concerns.
    // Backwards-compatible wrappers (thin) retained for external callers.
    static void exportAudioDataToWav(const AudioData& audioData, const std::string& path);

    static void writeIndexToFile(const boost::multiprecision::cpp_int& index,
                                 const std::string&                    outDir   = std::string(),
                                 const std::string&                    filename = std::string());

    // Metadata helpers
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

   private:
    AudioData     audioData;
    IndexMetadata metadata;
};

} // namespace AudioBabel

#endif // AUDIO_INDEX_H
