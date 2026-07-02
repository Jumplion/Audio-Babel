#ifndef FILE_IO_H
#define FILE_IO_H

#include <cstdint>
#include <string>
#include <vector>

namespace AudioBabel {

// Reads and writes PCM audio data to/from WAV files. FileIO is the only
// place sample rate, bit depth, and channel count are meaningful — Index
// works with raw PCM payload bytes only and knows nothing about files.
class FileIO {
   public:
    // Header fields + PCM payload for a WAV file. Plain POD so tests and
    // callers can construct/inspect it directly. samples is little-endian
    // per-sample, matching WAV file convention.
    struct AudioData {
        uint32_t             sample_rate;
        uint16_t             bit_rate;     // bits per sample (8, 16, or 32)
        uint16_t             num_channels; // 1 = mono, 2 = stereo
        uint16_t             audio_format; // 1 = PCM
        size_t               num_frames;
        std::vector<uint8_t> samples;
    };

    // Parses a RIFF/WAVE PCM file ('fmt ' chunk followed by 'data' chunk;
    // unknown chunks are skipped per RIFF padding rules). Supports PCM only,
    // 8/16/32-bit depths, any sample rate and channel count.
    // Throws std::runtime_error if the file can't be opened, the format is
    // invalid, the bit depth is unsupported, or the file is truncated.
    static auto readWav(const std::string& path) -> AudioData;

    static void writeWav(const AudioData& audioData, const std::string& path);

    // Writes a raw PCM payload using the fixed default header
    // (PCM, 44100 Hz, 16-bit, mono).
    static void writeWav(const std::vector<uint8_t>& samples, const std::string& path);
};

} // namespace AudioBabel

#endif // FILE_IO_H
