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

// --- Sample-domain repunit / band-index helpers --------------------------
// Shared by Index's payload bijection and IndexScramble's per-band
// keying, both of which work over base-B numbers with B = SAMPLE_ALPHABET_SIZE
// (the 16-bit sample alphabet) and need the same two primitives:
//   - the base-B repunit S_L = (B^L - 1)/(B - 1) for an L-sample band, and
//   - recovering L (the sample count) from a stored index magnitude.

// Base-B repunit S_L: byte pattern 0x00 0x01 repeated L times (every digit ==
// 1), most-significant-sample first. Built in one linear pass.
inline auto repunit(size_t L) -> boost::multiprecision::cpp_int {
    if (L == 0) {
        return boost::multiprecision::cpp_int(0);
    }
    constexpr size_t      sampleBytes = DEFAULT_BIT_DEPTH / BITS_PER_BYTE;
    std::vector<uint8_t>  bytes(L * sampleBytes, 0);
    for (size_t i = 0; i < L; ++i) {
        bytes[(i * sampleBytes) + 1] = 0x01;
    }
    boost::multiprecision::cpp_int s = 0;
    boost::multiprecision::import_bits(s, bytes.begin(), bytes.end(), BITS_PER_BYTE, true);
    return s;
}

// Band index L (== sample count) for a stored index value n: for an L-sample
// payload, n lies in [S_L, S_{L+1}-1], and with m = n*(B-1) + 1,
// L = floor(log_B(m)) = msb(m) / 16, recovered without bignum division.
inline auto bandIndex(const boost::multiprecision::cpp_int& n) -> size_t {
    if (n == 0) {
        return 0;
    }
    boost::multiprecision::cpp_int m = (n * (SAMPLE_ALPHABET_SIZE - 1)) + 1;
    return static_cast<size_t>(boost::multiprecision::msb(m) / DEFAULT_BIT_DEPTH);
}

// --- Avalanche bit-mixing primitives -----------------------------------------
// A small, fast bit mixer (one SplitMix64 step on a running state).
// Source: Steele, Lea & Flood, "Fast Splittable Pseudorandom Number
// Generators" (OOPSLA 2014) — https://gee.cs.oswego.edu/dl/papers/oopsla14.pdf
// Shared by IndexScramble's Feistel keying and IndexNaming's cosmetic name
// generation — both need a cheap, well-diffused mixer over a running state.
inline void mixIn(uint64_t& state, uint8_t x) {
    state += x + 0x9E3779B97F4A7C15ULL;
    state = (state ^ (state >> 30)) * 0xBF58476D1CE4E5B9ULL;
    state = (state ^ (state >> 27)) * 0x94D049BB133111EBULL;
    state ^= state >> 31;
}

inline auto splitmix64(uint64_t& state) -> uint64_t {
    uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z          = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z          = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

// --- Base64 URL-safe utilities (alphabet A-Z a-z 0-9 - _, no padding) ---------
// URL-safe base64 alphabet used by encoder/decoder (no padding)
constexpr char BASE64_URL_ALPHA[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// NOTE (R7): isValidBase64Url is intentionally implemented in both C++ (this
// character-loop version) and JavaScript (regex in docs/js/utils/base64.js).
// The dual implementations provide defence-in-depth: JS validates at the UI
// boundary while C++ validates at the library boundary, and each uses an
// idiomatic approach for its runtime.
inline auto isValidBase64Url(const std::string& s) -> bool {
    for (char c : s) {
        if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

// --- Bijective base-64 for the INDEX string form -----------------------------
// These implement a TRUE BIJECTION between non-negative integers
// and strings over the 64-symbol URL-safe alphabet using bijective numeration
// (digit = value + 1). Every string of any length >= 0 maps to exactly one
// integer and back, with the empty string <-> 0. Nothing is rejected for
// alphabet-valid input. Do NOT use the bit-accumulator base64 for indices.
// Reference: https://en.wikipedia.org/wiki/Bijective_numeration

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

// Number of bits per base-64 digit.
constexpr unsigned BASE64_DIGIT_BITS = 6; // 1 << 6 == 64

// integer -> index string (bijective base 64).
//
// Conceptually: while n > 0 { n -= 1; emit ALPHA[n mod 64]; n /= 64 } reversed.
// That per-digit loop is O(len^2). We use the exact identity instead (see the
// Index.cpp comments for the analogous base-B derivation):
//   n = V64 + S64,  V64 = base-64 value of the digit string (digits 0..63),
//                   S64 = (64^len - 1)/63 = base-64 repunit (all digits == 1).
// The digit count is len = msb(n*63 + 1) / 6, recovered without bignum division.
// All steps are linear in the output length.
inline auto indexToB64(const boost::multiprecision::cpp_int& n) -> std::string {
    if (n < 0) {
        throw std::invalid_argument("indexToB64: index must be non-negative");
    }
    if (n == 0) {
        return std::string();
    }

    // len = floor(log_64(n*(64-1) + 1)).
    boost::multiprecision::cpp_int m   = (n * (BASE64_ALPHABET_SIZE - 1)) + 1;
    size_t                         len = static_cast<size_t>(boost::multiprecision::msb(m) / BASE64_DIGIT_BITS);

    // S64 repunit: `len` digits all equal to 1.
    std::vector<uint8_t>           repunitDigits(len, 1);
    boost::multiprecision::cpp_int repunit = 0;
    boost::multiprecision::import_bits(repunit, repunitDigits.begin(), repunitDigits.end(), BASE64_DIGIT_BITS, true);

    // V64 = n - S64; its base-64 digits (most significant first) are the output.
    boost::multiprecision::cpp_int value = n - repunit;
    std::vector<uint8_t>           digits;
    boost::multiprecision::export_bits(value, std::back_inserter(digits), BASE64_DIGIT_BITS, true);

    std::string out(len, BASE64_URL_ALPHA[0]);
    // export_bits strips leading zero digits; align to the right of the output.
    size_t offset = (digits.size() <= len) ? (len - digits.size()) : 0;
    for (size_t i = 0; i < digits.size() && offset + i < len; ++i) {
        out[offset + i] = BASE64_URL_ALPHA[digits[i]];
    }
    return out;
}

// index string -> integer.
//
// Conceptually: n = 0; for each char c: n = n*64 + (alphaValue(c) + 1).
// Using the same identity, n = V64 + S64 where V64 is built from the 6-bit digit
// values in one linear import_bits pass and S64 is the base-64 repunit.
inline auto b64ToIndex(const std::string& s) -> boost::multiprecision::cpp_int {
    if (s.empty()) {
        return boost::multiprecision::cpp_int(0);
    }

    std::vector<uint8_t> digits(s.size());
    std::vector<uint8_t> repunitDigits(s.size(), 1);
    for (size_t i = 0; i < s.size(); ++i) {
        int v = base64UrlValue(s[i]);
        if (v < 0) {
            throw std::invalid_argument("b64ToIndex: invalid base64 character in index");
        }
        digits[i] = static_cast<uint8_t>(v);
    }

    boost::multiprecision::cpp_int value   = 0;
    boost::multiprecision::cpp_int repunit = 0;
    boost::multiprecision::import_bits(value, digits.begin(), digits.end(), BASE64_DIGIT_BITS, true);
    boost::multiprecision::import_bits(repunit, repunitDigits.begin(), repunitDigits.end(), BASE64_DIGIT_BITS, true);
    return value + repunit;
}

} // namespace AudioBabel::Utilities

#endif // AUDIOBABEL_UTILITIES_H