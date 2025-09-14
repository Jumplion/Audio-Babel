#ifndef AUDIOBABEL_UTILITIES_H
#define AUDIOBABEL_UTILITIES_H

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <ostream>
#include <vector>

// Utilities: endian helpers and small byte/bit helpers used across the audio code.
// Header-only and inline to avoid ODR issues.
namespace AudioBabel::Utilities {

// --- Endian helpers (LE) -----------------------------------------------------------------
template <typename T>
inline void write_le(std::ostream& out, T value) {
    std::array<uint8_t, sizeof(T)> buf{};
    for (size_t index = 0; index < sizeof(T); ++index) {
        buf[index] = static_cast<uint8_t>((value >> (index * 8)) & 0xFFU);
    }
    out.write(reinterpret_cast<const char*>(buf.data()), sizeof(T));
}

inline void write_u16_le(std::ostream& out, uint16_t value) {
    write_le<uint16_t>(out, value);
}
inline void write_u32_le(std::ostream& out, uint32_t value) {
    write_le<uint32_t>(out, value);
}

template <typename T>
inline auto read_le(const char* ptr) -> T {
    uint64_t acc = 0;
    for (size_t index = 0; index < sizeof(T); ++index) {
        acc |= (static_cast<uint64_t>(static_cast<uint8_t>(ptr[index])) << (index * 8));
    }
    return static_cast<T>(acc);
}

inline auto read_u16_le(const char* ptr) -> uint16_t {
    return read_le<uint16_t>(ptr);
}
inline auto read_u32_le(const char* ptr) -> uint32_t {
    return read_le<uint32_t>(ptr);
}

// --- Big-endian helpers and byte utilities -----------------------------------------------

// Note: these use bit/byte constants expected to be defined in Constants.h (import where used)
// Helper: compare 4-byte ASCII chunk tags (e.g., "RIFF", "WAVE")
static inline auto tagEquals(const std::array<char, 4>& tagBuf, const char expected[4]) -> bool {
    for (size_t i = 0; i < 4; ++i) {
        if (tagBuf[i] != expected[i]) {
            return false;
        }
    }
    return true;
}

static inline void push_be_u16(std::vector<uint8_t>& out, uint16_t v, size_t BITS_PER_BYTE = 8, uint32_t BYTE_MASK = 0xFFU) {
    out.push_back(static_cast<uint8_t>((v >> 8) & BYTE_MASK));
    out.push_back(static_cast<uint8_t>((v >> 0) & BYTE_MASK));
}

static inline void push_be_u32(std::vector<uint8_t>& out, uint32_t v, size_t BITS_PER_BYTE = 8, uint32_t BYTE_MASK = 0xFFU) {
    for (int i = 3; i >= 0; --i) {
        out.push_back(static_cast<uint8_t>((v >> (i * BITS_PER_BYTE)) & BYTE_MASK));
    }
}

static inline void push_be_u64(std::vector<uint8_t>& out, uint64_t v, size_t BITS_PER_BYTE = 8, uint32_t BYTE_MASK = 0xFFU) {
    for (int i = 7; i >= 0; --i) {
        out.push_back(static_cast<uint8_t>((v >> (i * BITS_PER_BYTE)) & BYTE_MASK));
    }
}

static inline auto parse_be_u16(const std::vector<uint8_t>& buf, size_t offset, size_t BITS_PER_BYTE = 8) -> uint16_t {
    return static_cast<uint16_t>((static_cast<uint16_t>(buf[offset]) << 8) | static_cast<uint16_t>(buf[offset + 1]));
}

static inline auto parse_be_u32(const std::vector<uint8_t>& buf, size_t offset, size_t BITS_PER_BYTE = 8) -> uint32_t {
    return (static_cast<uint32_t>(buf[offset]) << 24) | (static_cast<uint32_t>(buf[offset + 1]) << 16) |
           (static_cast<uint32_t>(buf[offset + 2]) << 8) | static_cast<uint32_t>(buf[offset + 3]);
}

static inline auto parse_be_u64(const std::vector<uint8_t>& buf, size_t offset, size_t BITS_PER_BYTE = 8) -> uint64_t {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v = (v << BITS_PER_BYTE) | static_cast<uint64_t>(buf[offset + i]);
    }
    return v;
}

static inline void append_sample_be_from_le(const std::vector<uint8_t>& le_samples,
                                            size_t                      offset,
                                            size_t                      bytes_per_sample,
                                            std::vector<uint8_t>&       out) {
    for (int byteIndex = static_cast<int>(bytes_per_sample) - 1; byteIndex >= 0; --byteIndex) {
        out.push_back(le_samples[offset + byteIndex]);
    }
}

static inline auto bytes_to_cpp_int_be(const std::vector<uint8_t>& bytes) -> boost::multiprecision::cpp_int {
    boost::multiprecision::cpp_int res = 0;
    for (uint8_t b : bytes) {
        res <<= 8;
        res |= boost::multiprecision::cpp_int(static_cast<uint32_t>(b));
    }
    return res;
}

} // namespace AudioBabel::Utilities

#endif // AUDIOBABEL_UTILITIES_H