#include "Index.h"

#include <algorithm>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <vector>

#include "Constants.h"
#include "IndexScramble.h"
#include "Utilities.h"

using boost::multiprecision::cpp_int;

namespace AudioBabel {

using namespace Utilities;

// Bytes per PCM sample at the default bit depth (2 for 16-bit).
static constexpr size_t SAMPLE_BYTES = DEFAULT_BIT_DEPTH / BITS_PER_BYTE;

auto Index::encode(const std::vector<uint8_t>& samples) -> boost::multiprecision::cpp_int {
    // Bijective numeration (digit = value + 1) applied per-sample naively is
    // n = n*B + (v+1) in a loop, which is O(L^2) in bignum arithmetic. Instead
    // we use the closed form: with digit d_i = v_i + 1,
    //   n = Sum_i (v_i + 1) B^(L-1-i) = V + S_L
    // where V is the payload read as one big base-B number (built in one
    // import_bits pass) and S_L is the base-B repunit (every digit == 1). A
    // single bignum addition propagates the per-sample +1 carries, so this is
    // O(N) with no per-sample loop.
    const auto&  bytes = samples;
    const size_t L     = (bytes.size() + (SAMPLE_BYTES - 1)) / SAMPLE_BYTES; // whole samples (ceil)

    cpp_int index = 0;
    if (L != 0) {
        // Payload bytes (V), most-significant-sample first for import_bits(msv=true).
        // S_L (the repunit) comes from the shared helper in Utilities.h.
        std::vector<uint8_t> valueBytes(L * SAMPLE_BYTES, 0);
        for (size_t i = 0; i < L; ++i) {
            size_t lo = i * SAMPLE_BYTES;
            // A stray trailing byte (should not occur for 16-bit data) is treated
            // as a zero byte so no value is silently dropped.
            size_t hi = lo + 1 < bytes.size() ? lo + 1 : lo;

            valueBytes[lo]     = bytes[lo];
            valueBytes[lo + 1] = bytes[hi];
        }

        cpp_int value = 0;
        boost::multiprecision::import_bits(value, valueBytes.begin(), valueBytes.end(), BITS_PER_BYTE, true);
        index = value + repunit(L);
    }

    // Optional reversible scramble so similar payloads land far apart (and short
    // indices reach a wider range of lengths). It is a bijection within each
    // length-tier, so it is identity-safe when disabled and never breaks the
    // round-trip when enabled. See IndexScramble.h.
    const IndexScramble::Config& scrambleCfg = IndexScramble::config();
    if (scrambleCfg.enabled) {
        index = IndexScramble::scramble(index, scrambleCfg.seed);
    }

    return index;
}

auto Index::decode(const boost::multiprecision::cpp_int& index) -> std::vector<uint8_t> {
    // Inverse of encode via n = V + S_L. Every alphabet-valid index decodes;
    // there is intentionally no integrity check. The sample count L is
    // recovered without bignum division: for an L-sample payload, n lies in
    // [S_L, S_{L+1}-1], and with m = n*(B-1) + 1, L = msb(m) / 16 (see
    // Utilities::bandIndex). Then V = n - S_L, and its L base-B digits are the
    // samples (most significant first).

    // Undo the optional reversible scramble (identity unless enabled) before
    // decoding. The stored index is what carries the scramble.
    const IndexScramble::Config& scrambleCfg = IndexScramble::config();
    const cpp_int                idx         = scrambleCfg.enabled ? IndexScramble::unscramble(index, scrambleCfg.seed) : index;

    std::vector<uint8_t> samples;

    if (idx > 0) {
        // Sample count L and the S_L repunit come from the shared helpers in
        // Utilities.h (also used by IndexScramble for the same length math).
        size_t L = bandIndex(idx);

        // V = n - S_L is the base-B value of the samples (V < B^L).
        cpp_int              value = idx - repunit(L);
        std::vector<uint8_t> valueBytes;
        boost::multiprecision::export_bits(value, std::back_inserter(valueBytes), BITS_PER_BYTE, true);

        // Left-pad to exactly L*SAMPLE_BYTES (export strips leading zero bytes).
        std::vector<uint8_t> padded(L * SAMPLE_BYTES, 0);
        if (valueBytes.size() <= padded.size()) {
            std::copy(valueBytes.begin(), valueBytes.end(), padded.end() - static_cast<std::ptrdiff_t>(valueBytes.size()));
        }

        samples = padded;
    }

    return samples;
}

} // namespace AudioBabel
