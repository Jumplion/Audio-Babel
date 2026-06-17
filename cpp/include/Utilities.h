#ifndef AUDIOBABEL_UTILITIES_H
#define AUDIOBABEL_UTILITIES_H

#include <algorithm>
#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <ostream>
#include <stdexcept>
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

// --- Byte/tag utilities -------------------------------------------------------------------

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
static inline void push_le(std::vector<uint8_t>& out, T val) {
    auto uv = static_cast<uint64_t>(val);
    for (size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(static_cast<uint8_t>((uv >> (i * 8)) & 0xFFU));
    }
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

// --- Bijective base-64 for the INDEX string form -----------------------------
// Unlike encodeBase64Url/decodeBase64Url (which pack bits across byte
// boundaries), these implement a TRUE BIJECTION between non-negative integers
// and strings over the 64-symbol URL-safe alphabet using bijective numeration
// (digit = value + 1). Every string of any length >= 0 maps to exactly one
// integer and back, with the empty string <-> 0. Nothing is rejected for
// alphabet-valid input. Do NOT use the bit-accumulator base64 for indices.

// Map a single alphabet character to its 0..63 value, or -1 if not in the alphabet.
inline auto base64UrlValue(char c) -> int {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return (c - 'a') + 26;
    }
    if (c >= '0' && c <= '9') {
        return (c - '0') + 52;
    }
    if (c == '-') {
        return 62;
    }
    if (c == '_') {
        return 63;
    }
    return -1;
}

// integer -> index string (bijective base 64):
//   while n > 0: { n -= 1; emit ALPHA[n mod 64]; n = n / 64 }  // then reverse
inline auto indexToB64(boost::multiprecision::cpp_int n) -> std::string {
    if (n < 0) {
        throw std::invalid_argument("indexToB64: index must be non-negative");
    }
    std::string out;
    while (n > 0) {
        n -= 1;
        auto digit = static_cast<unsigned>(n % BASE64_ALPHABET_SIZE);
        out.push_back(BASE64_URL_ALPHA[digit]);
        n /= BASE64_ALPHABET_SIZE;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

// index string -> integer:
//   n = 0; for each char c in order: n = n*64 + (alphaValue(c) + 1)
inline auto b64ToIndex(const std::string& s) -> boost::multiprecision::cpp_int {
    boost::multiprecision::cpp_int n = 0;
    for (char c : s) {
        int v = base64UrlValue(c);
        if (v < 0) {
            throw std::invalid_argument("b64ToIndex: invalid base64 character in index");
        }
        n = (n * BASE64_ALPHABET_SIZE) + (v + 1);
    }
    return n;
}

} // namespace AudioBabel::Utilities

#endif // AUDIOBABEL_UTILITIES_H