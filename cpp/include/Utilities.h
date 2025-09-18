#ifndef AUDIOBABEL_UTILITIES_H
#define AUDIOBABEL_UTILITIES_H

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// Utilities: endian helpers and small byte/bit helpers used across the audio code.
// Header-only and inline to avoid ODR issues.
namespace AudioBabel::Utilities {

// --- Little-Endian Write helpers ----------------------------------------------------------
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

// --- Little-endian Read helpers -----------------------------------------------------------
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

// --- Base64 URL-safe utilities (alphabet A-Z a-z 0-9 - _, no padding) ---------
// Accepts string-like inputs via std::string_view.
constexpr auto isValidBase64Url(std::string_view s) -> bool {
    for (char c : s) {
        if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

// Decode URL-safe base64 (no padding) into bytes. Throws std::invalid_argument on invalid input.
inline auto decodeBase64Url(const std::string& s) -> std::vector<uint8_t> {
    static const std::array<int8_t, 256> rev = []() {
        std::array<int8_t, 256> table{};
        table.fill(-1);
        const std::string alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        for (size_t i = 0; i < alpha.size(); ++i) {
            table[static_cast<unsigned char>(alpha[i])] = static_cast<int8_t>(i);
        }
        return table;
    }();

    std::vector<uint8_t> out;
    uint32_t             acc      = 0;
    int                  acc_bits = 0;
    for (char ch : s) {
        int8_t v = rev[static_cast<unsigned char>(ch)];
        if (v < 0) {
            throw std::invalid_argument("Invalid base64 character in input");
        }
        acc = (acc << 6) | static_cast<uint32_t>(v);
        acc_bits += 6;
        if (acc_bits >= 8) {
            acc_bits -= 8;
            auto b = static_cast<uint8_t>((acc >> acc_bits) & 0xFF);
            out.push_back(b);
        }
    }
    return out;
}

// Encode bytes into URL-safe base64 (no padding)
inline auto encodeBase64Url(const std::vector<uint8_t>& bytes) -> std::string {
    static const char b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string       b64str;
    b64str.reserve((bytes.size() * 8 + 5) / 6);
    uint32_t acc      = 0;
    int      acc_bits = 0;
    for (uint8_t byte : bytes) {
        acc = (acc << 8) | byte;
        acc_bits += 8;
        while (acc_bits >= 6) {
            acc_bits -= 6;
            auto idx = static_cast<uint8_t>((acc >> acc_bits) & 0x3F);
            b64str.push_back(b64_alpha[idx]);
        }
    }
    if (acc_bits > 0) {
        auto idx = static_cast<uint8_t>((acc << (6 - acc_bits)) & 0x3F);
        b64str.push_back(b64_alpha[idx]);
    }
    return b64str;
}

} // namespace AudioBabel::Utilities

#endif // AUDIOBABEL_UTILITIES_H