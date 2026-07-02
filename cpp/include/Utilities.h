#ifndef AUDIOBABEL_UTILITIES_H
#define AUDIOBABEL_UTILITIES_H

#include <algorithm>
#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Constants.h"

// Endian helpers and small byte/bit helpers shared across the audio code.
// Header-only and inline to avoid ODR issues.
namespace AudioBabel::Utilities {

// --- Little-endian read/write ---------------------------------------------
template <typename T>
inline void write_le(std::ostream& out, T value) {
    std::array<uint8_t, sizeof(T)> buf{};
    for (size_t index = 0; index < sizeof(T); ++index) {
        buf[index] = static_cast<uint8_t>((value >> (index * 8)) & 0xFFU);
    }
    out.write(reinterpret_cast<const char*>(buf.data()), sizeof(T));
}

template <typename T>
inline auto read_le(const char* ptr) -> T {
    uint64_t acc = 0;
    for (size_t index = 0; index < sizeof(T); ++index) {
        acc |= (static_cast<uint64_t>(static_cast<uint8_t>(ptr[index])) << (index * 8));
    }
    return static_cast<T>(acc);
}

// --- Byte/tag utilities -----------------------------------------------------

// Compares 4-byte ASCII chunk tags (e.g. "RIFF", "WAVE").
static inline auto tagEquals(const std::array<char, 4>& tagBuf, const char expected[4]) -> bool {
    for (size_t i = 0; i < 4; ++i) {
        if (tagBuf[i] != expected[i]) {
            return false;
        }
    }
    return true;
}

// --- Sample-domain repunit / band-index helpers -----------------------------
// Shared by Index's payload bijection and IndexScramble's per-band keying,
// both working over base-B numbers (B = SAMPLE_ALPHABET_SIZE).

// Base-B repunit S_L for an L-sample band: byte pattern 0x00 0x01 repeated L
// times (every digit == 1), most-significant-sample first.
inline auto repunit(size_t L) -> boost::multiprecision::cpp_int {
    if (L == 0) {
        return boost::multiprecision::cpp_int(0);
    }
    // encode()/decode() and contentScramble() request the same L back-to-back,
    // so a single-entry cache turns two O(N) builds into one.
    static thread_local size_t                         cachedL = 0;
    static thread_local boost::multiprecision::cpp_int cached;
    if (L == cachedL) {
        return cached;
    }
    constexpr size_t     sampleBytes = DEFAULT_BIT_DEPTH / BITS_PER_BYTE;
    std::vector<uint8_t> bytes(L * sampleBytes, 0);
    for (size_t i = 0; i < L; ++i) {
        bytes[(i * sampleBytes) + 1] = 0x01;
    }
    boost::multiprecision::cpp_int s = 0;
    boost::multiprecision::import_bits(s, bytes.begin(), bytes.end(), BITS_PER_BYTE, true);
    cachedL = L;
    cached  = s;
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

// --- Avalanche bit-mixing primitives ----------------------------------------
// One SplitMix64 step on a running state (Steele, Lea & Flood, "Fast
// Splittable Pseudorandom Number Generators", OOPSLA 2014). Shared by
// IndexScramble's Feistel keying and IndexNaming's name generation.
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

// --- Balanced big-integer Feistel permutation primitives --------------------
// Shared by IndexScramble's per-band scatter and IndexNaming's cosmetic-name
// permutation: both build a keyed bijection over [0, 2^e) (e even) from these
// pieces, differing only in how each round's key is derived. LibraryPosition
// keeps its own uint64-domain variant since its 14-bit domain is faster in
// machine integers than cpp_int.

// Four rounds give full avalanche across the domain.
constexpr int FEISTEL_ROUNDS = 4;

inline auto lowBitsMask(size_t h) -> boost::multiprecision::cpp_int {
    return (boost::multiprecision::cpp_int(1) << h) - 1;
}

// Keyed diffusing byte mixer: half-block -> half-block of the same length.
// Need not be invertible itself — the Feistel structure provides that.
inline auto feistelDiffuse(const std::vector<uint8_t>& in, uint64_t key) -> std::vector<uint8_t> {
    const size_t         n = in.size();
    std::vector<uint8_t> out(n, 0);

    uint64_t fwd = key ^ 0xA0761D6478BD642FULL;
    for (size_t i = 0; i < n; ++i) {
        mixIn(fwd, in[i]);
        out[i] = static_cast<uint8_t>(fwd);
    }

    uint64_t bwd = key ^ 0xE7037ED1A0B428DBULL;
    for (size_t i = n; i-- > 0;) {
        mixIn(bwd, static_cast<uint8_t>(in[i] ^ out[i]));
        out[i] = static_cast<uint8_t>(out[i] ^ static_cast<uint8_t>(bwd >> 17));
    }
    return out;
}

// h-bit keyed round function built on feistelDiffuse: the h-bit half is laid
// out in ceil(h/8) bytes (MSB first) and the result masked back to h bits.
inline auto feistelRoundBits(const boost::multiprecision::cpp_int& half,
                             size_t                                h,
                             uint64_t                              key,
                             const boost::multiprecision::cpp_int& mask) -> boost::multiprecision::cpp_int {
    const size_t         hbytes = (h + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
    std::vector<uint8_t> in(hbytes, 0);

    std::vector<uint8_t> raw;
    boost::multiprecision::export_bits(half, std::back_inserter(raw), BITS_PER_BYTE, true);
    if (raw.size() <= in.size()) {
        std::copy(raw.begin(), raw.end(), in.end() - static_cast<std::ptrdiff_t>(raw.size()));
    }

    std::vector<uint8_t>           out = feistelDiffuse(in, key);
    boost::multiprecision::cpp_int r   = 0;
    boost::multiprecision::import_bits(r, out.begin(), out.end(), BITS_PER_BYTE, true);
    return r & mask;
}

// Keyed balanced Feistel permutation over [0, 2^e) (e even). `roundKey(r)`
// supplies each round's key; inverted by running rounds in reverse.
template <typename RoundKeyFn>
inline auto feistelPow2(const boost::multiprecision::cpp_int& x, size_t e, RoundKeyFn&& roundKey, bool encrypt) -> boost::multiprecision::cpp_int {
    using boost::multiprecision::cpp_int;
    const size_t  h    = e / 2;
    const cpp_int mask = lowBitsMask(h);
    cpp_int       hi   = (x >> h) & mask;
    cpp_int       lo   = x & mask;

    if (encrypt) {
        for (int r = 0; r < FEISTEL_ROUNDS; ++r) {
            cpp_int f = feistelRoundBits(lo, h, roundKey(r), mask);
            cpp_int t = hi ^ f;
            hi        = lo;
            lo        = t;
        }
    } else {
        for (int r = FEISTEL_ROUNDS - 1; r >= 0; --r) {
            cpp_int f = feistelRoundBits(hi, h, roundKey(r), mask);
            cpp_int t = lo ^ f;
            lo        = hi;
            hi        = t;
        }
    }
    return (hi << h) | lo;
}

// Reduces a (possibly multi-megabyte) non-negative index modulo `modulus` in
// O(N) time. `modulus` must not be a power of two: a power-of-two modulus
// would make the result depend only on the index's low bits, whereas an
// arbitrary modulus mixes every bit into the residue via Horner's rule below
// — needed because sibling library positions differ only in low bits and
// still must land on well-separated residues.
//
// Plain `idx % modulus` is an O(N^2) trap: boost's cpp_int division forms the
// full quotient even though only the residue is wanted. Folding in fixed-width
// chunks (Horner's rule, base 2^CHUNK_BITS) keeps each step's work O(1) for
// an overall O(N); the result is bit-identical to `idx % modulus`.
inline auto reduceModLarge(const boost::multiprecision::cpp_int& idx,
                           const boost::multiprecision::cpp_int& modulus) -> boost::multiprecision::cpp_int {
    using boost::multiprecision::cpp_int;
    if (idx < modulus) {
        return idx;
    }
    // Most-significant-byte-first bytes of the index (one linear pass).
    std::vector<uint8_t> bytes;
    boost::multiprecision::export_bits(idx, std::back_inserter(bytes), BITS_PER_BYTE, true);

    constexpr size_t CHUNK_BYTES = 32; // 256-bit folding step (comfortably < modulus's width)
    cpp_int          rem         = 0;
    const size_t     n           = bytes.size();
    for (size_t i = 0; i < n;) {
        const size_t take  = std::min(CHUNK_BYTES, n - i);
        cpp_int      chunk = 0;
        for (size_t k = 0; k < take; ++k) {
            chunk = (chunk << 8) | cpp_int(bytes[i + k]);
        }
        rem = ((rem << (take * 8)) + chunk) % modulus;
        i += take;
    }
    return rem;
}

// --- Base64 URL-safe utilities (alphabet A-Z a-z 0-9 - _, no padding) -------
constexpr char BASE64_URL_ALPHA[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// Also implemented in JS (docs/js/utils/base64.js) as defence-in-depth: JS
// validates at the UI boundary, C++ at the library boundary.
inline auto isValidBase64Url(const std::string& s) -> bool {
    for (char c : s) {
        if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

// --- Bijective base-64 for the INDEX string form ----------------------------
// True bijection between non-negative integers and strings over the 64-symbol
// URL-safe alphabet using bijective numeration (digit = value + 1): every
// string of any length >= 0 maps to exactly one integer and back, empty
// string <-> 0, nothing rejected for alphabet-valid input. Do NOT use the
// bit-accumulator base64 for indices. https://en.wikipedia.org/wiki/Bijective_numeration

// Maps a single alphabet character to its 0..63 value, or -1 if not in the alphabet.
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

constexpr unsigned BASE64_DIGIT_BITS = 6; // 1 << 6 == 64

// integer -> index string (bijective base 64).
//
// Naively: while n > 0 { n -= 1; emit ALPHA[n mod 64]; n /= 64 } reversed —
// O(len^2). Instead uses the closed-form n = V64 + S64, where V64 is the
// base-64 value of the digit string and S64 = (64^len - 1)/63 is the base-64
// repunit; len = msb(n*63 + 1) / 6. All steps are linear in the output length.
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

// index string -> integer. Same identity as indexToB64: n = V64 + S64, with
// V64 built from the 6-bit digit values in one linear import_bits pass.
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
