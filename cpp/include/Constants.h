#ifndef AUDIOBABEL_CONSTANTS_H
#define AUDIOBABEL_CONSTANTS_H

#include <cstdint>

namespace AudioBabel {

// Centralized constants used across the cpp implementation.

// Keep these in a header so they can be reused by multiple translation units.
constexpr uint32_t CHUNK_SIZE_LIMIT = (1U << 30); // 1 GiB sanity limit for chunk sizes
// WAV / RIFF constants
constexpr size_t WAV_ID_LEN         = 4;  // 'RIFF'/'WAVE' id length
constexpr size_t FMT_CHUNK_MIN_SIZE = 16; // canonical PCM fmt chunk size
constexpr size_t BITS_PER_BYTE      = 8;

// Base64 / byte masks
constexpr int      BASE64_BITS = 6;     // bits per base64 digit in our table
constexpr uint32_t BYTE_MASK   = 0xFFU; // mask for a single byte (255)
constexpr uint32_t BASE64_MASK = 0x3FU; // mask for 6-bit base64 values (63)

// WAV format defaults and limits
constexpr uint16_t PCM_FORMAT_CODE        = 1;  // PCM format value
constexpr uint16_t DEFAULT_NUM_CHANNELS   = 1;  // default assumed channels for sample vectors
constexpr uint32_t WAV_FILE_BASE_OVERHEAD = 36; // base size used in RIFF size field

// Supported PCM bit depths
constexpr std::array<uint16_t, 3> PCM_BITS_PER_SAMPLE = {8, 16, 32}; // bits per sample for supported depths

} // namespace AudioBabel

#endif // AUDIOBABEL_CONSTANTS_H
