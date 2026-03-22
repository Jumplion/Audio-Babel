#ifndef AUDIOBABEL_UTILITIES_H
#define AUDIOBABEL_UTILITIES_H

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "Constants.h"

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

// --- Little-endian Read helpers -----------------------------------------------------------
template <typename T>
inline auto read_le(const char* ptr) -> T {
    uint64_t acc = 0;
    for (size_t index = 0; index < sizeof(T); ++index) {
        acc |= (static_cast<uint64_t>(static_cast<uint8_t>(ptr[index])) << (index * 8));
    }
    return static_cast<T>(acc);
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

template <typename T>
static inline void push_be(std::vector<uint8_t>& out, T val, size_t BITS_PER_BYTE = 8, uint32_t BYTE_MASK = 0xFFU) {
    // Cast to a known-width unsigned type for safe shifting
    auto      uv        = static_cast<uint64_t>(val);
    const int num_bytes = static_cast<int>(sizeof(T));
    for (int i = num_bytes - 1; i >= 0; --i) {
        out.push_back(static_cast<uint8_t>((uv >> (i * BITS_PER_BYTE)) & BYTE_MASK));
    }
}

template <typename T>
static inline void push_le(std::vector<uint8_t>& out, T val) {
    auto uv = static_cast<uint64_t>(val);
    for (size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(static_cast<uint8_t>((uv >> (i * 8)) & 0xFFU));
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
// URL-safe base64 alphabet used by encoder/decoder (no padding)
constexpr char BASE64_URL_ALPHA[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// NOTE (R7): isValidBase64Url is intentionally implemented in both C++ (this
// character-loop version) and JavaScript (regex in docs/js/utils/audioIndex.js).
// The dual implementations provide defence-in-depth: JS validates at the UI
// boundary while C++ validates at the library boundary, and each uses an
// idiomatic approach for its runtime.
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
        const char* alpha = BASE64_URL_ALPHA;
        for (size_t i = 0; i < 64; ++i) {
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
    static const char* b64_alpha = BASE64_URL_ALPHA;
    std::string        b64str;
    b64str.reserve((bytes.size() * AudioBabel::BITS_PER_BYTE + (AudioBabel::BASE64_BITS - 1)) / AudioBabel::BASE64_BITS);
    uint32_t acc      = 0;
    int      acc_bits = 0;
    for (uint8_t byte : bytes) {
        acc = (acc << AudioBabel::BITS_PER_BYTE) | byte;
        acc_bits += AudioBabel::BITS_PER_BYTE;
        while (acc_bits >= AudioBabel::BASE64_BITS) {
            acc_bits -= AudioBabel::BASE64_BITS;
            auto idx = static_cast<uint8_t>((acc >> acc_bits) & AudioBabel::BASE64_MASK);
            b64str.push_back(b64_alpha[idx]);
        }
    }
    if (acc_bits > 0) {
        auto idx = static_cast<uint8_t>((acc << (AudioBabel::BASE64_BITS - acc_bits)) & AudioBabel::BASE64_MASK);
        b64str.push_back(b64_alpha[idx]);
    }
    return b64str;
}

} // namespace AudioBabel::Utilities

#endif // AUDIOBABEL_UTILITIES_H