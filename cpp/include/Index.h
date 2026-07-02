#ifndef INDEX_H
#define INDEX_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <vector>

namespace AudioBabel {

// Bidirectional bijection between a raw PCM sample payload and a big integer.
// No header, version, or format metadata is embedded — see FileIO for
// reading/writing PCM payloads to/from WAV files. Encode/decode, the
// bijective base-64 index string, and the "no integrity check" property are
// documented in cpp/include/README.md and docs/INDEX_FORMAT.md.
//
// All static methods are thread-safe.
class Index {
   public:
    // Reads samples as little-endian 16-bit (B-ary) values and applies the
    // bijective payload->integer mapping (digit = value + 1). Depends only on
    // the sample values; never throws — there is nothing to validate.
    //
    // O(N) in the payload size: built via the closed-form identity n = V + S_L
    // (payload value plus the base-B repunit) with linear passes, not a
    // per-sample bignum loop.
    static auto encode(const std::vector<uint8_t>& samples) -> boost::multiprecision::cpp_int;

    // Inverse of encode(). Serializes each decoded 16-bit sample little-endian.
    // Never throws on an alphabet-valid index (no integrity check); trailing
    // zero (silence) samples are preserved exactly.
    //
    // O(N) in the payload size: sample count and digits are recovered from the
    // same n = V + S_L identity, with no per-sample bignum division.
    static auto decode(const boost::multiprecision::cpp_int& index) -> std::vector<uint8_t>;
};

} // namespace AudioBabel

#endif // INDEX_H
