#ifndef FILE_IO_H
#define FILE_IO_H

#include <cstdint>
#include <string>
#include <vector>

namespace AudioBabel {

/**
 * @class FileIO
 * @brief Reads and writes PCM audio data to/from WAV files.
 *
 * FileIO owns the WAV file format: the AudioData struct (header fields + PCM payload),
 * parsing a RIFF/WAVE file into it, and serializing it back out. It is the only place
 * sample rate, bit depth, and channel count are meaningful — Index works with raw PCM
 * payload bytes only and knows nothing about files or headers.
 *
 * @see Index for the PCM payload <-> big integer bijection
 */
class FileIO {
   public:
    /**
     * @struct AudioData
     * @brief Container for a WAV file's header fields and PCM sample payload.
     *
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

    /**
     * @brief Read a WAV file and extract its header fields and PCM payload.
     *
     * Parses a standard RIFF/WAVE PCM file and populates an AudioData structure.
     * The function expects a well-formed WAV file with a 'fmt ' chunk followed by
     * a 'data' chunk. Unknown chunks are skipped according to RIFF padding rules.
     *
     * @param path File path to the .wav file
     * @return AudioData structure with parsed header fields and audio samples
     * @throws std::runtime_error if file cannot be opened, format is invalid,
     *         bit depth is unsupported, or file is truncated
     *
     * @par Supported Formats
     * - PCM format only (audio_format = 1)
     * - Bit depths: 8, 16, or 32 bits per sample
     * - Any sample rate
     * - Any channel count
     */
    static auto readWav(const std::string& path) -> AudioData;

    /**
     * @brief Write a fully-specified AudioData structure to a WAV file.
     * @param audioData Header fields and PCM payload to write
     * @param path Output file path
     */
    static void writeWav(const AudioData& audioData, const std::string& path);

    /**
     * @brief Write a raw PCM payload to a WAV file using the fixed default header
     *        (PCM, 44100 Hz, 16-bit, mono).
     * @param samples PCM sample payload bytes (little-endian per-sample)
     * @param path Output file path
     */
    static void writeWav(const std::vector<uint8_t>& samples, const std::string& path);
};

} // namespace AudioBabel

#endif // FILE_IO_H
