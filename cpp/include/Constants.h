#ifndef AUDIOBABEL_CONSTANTS_H
#define AUDIOBABEL_CONSTANTS_H

#include <array>
#include <cstdint>

namespace AudioBabel {

// Centralized constants shared across translation units.

constexpr uint32_t CHUNK_SIZE_LIMIT = (1U << 30); // 1 GiB sanity limit for chunk sizes

// WAV / RIFF constants
constexpr size_t WAV_ID_LEN         = 4;  // 'RIFF'/'WAVE' id length
constexpr size_t FMT_CHUNK_MIN_SIZE = 16; // canonical PCM fmt chunk size
constexpr size_t BITS_PER_BYTE      = 8;

constexpr uint32_t BYTE_MASK = 0xFFU;

// WAV format defaults and limits
constexpr uint16_t PCM_FORMAT_CODE        = 1; // PCM format value
constexpr uint16_t DEFAULT_NUM_CHANNELS   = 1;
constexpr uint32_t WAV_FILE_BASE_OVERHEAD = 36; // base size used in RIFF size field

// Bijective index scheme (payload-only): no header, version, frame count,
// sample rate, bit depth, or channel count is stored in the index. Decoding
// applies a fixed default header: PCM, 44100 Hz, 16-bit, 1 channel (mono).
constexpr uint16_t DEFAULT_BIT_DEPTH   = 16;
constexpr uint32_t DEFAULT_SAMPLE_RATE = 44100;

// Sample alphabet size B = 1u << DEFAULT_BIT_DEPTH; tracks the bit depth so
// the bijection stays whole-sample aligned.
constexpr uint32_t SAMPLE_ALPHABET_SIZE = 1U << DEFAULT_BIT_DEPTH; // B = 65536

// Bijective base-64 over the URL-safe alphabet used for the index string form.
constexpr int BASE64_ALPHABET_SIZE = 64;

// Supported PCM bit depths
constexpr std::array<uint16_t, 3> PCM_BITS_PER_SAMPLE = {8, 16, 32};

} // namespace AudioBabel

#endif // AUDIOBABEL_CONSTANTS_H
