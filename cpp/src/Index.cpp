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
    /**
     * Payload-only bijection (samples -> integer), O(N) closed form.
     *
     * Each PCM sample is one base-B digit, B = SAMPLE_ALPHABET_SIZE (65536 at
     * 16-bit). This is bijective numeration (digit = value + 1):
     *   n = 0; for each sample v: n = n*B + (v + 1)
     *
     * That per-sample loop is O(L^2) in bignum arithmetic, so we use the
     * equivalent closed form instead. With digit d_i = v_i + 1:
     *   n = Sum_i (v_i + 1) B^(L-1-i) = Sum_i v_i B^(L-1-i) + Sum_j B^j = V + S_L
     * where:
     *   - V is the payload read as a base-B number (first sample most
     *     significant), i.e. the sample bytes big-endian per sample, built in
     *     one linear import_bits pass.
     *   - S_L = (B^L - 1)/(B - 1) is the base-B repunit (every digit == 1),
     *     byte pattern L copies of 0x00 0x01, built in one linear pass.
     * A single bignum addition propagates the per-sample (+1) carries, so the
     * whole operation is O(N) with no per-sample loop.
     *
     * Since every digit is value+1, a trailing zero sample is a real digit and
     * is preserved (k vs k+1 trailing zeros give different indices).
     */
    const auto&  bytes = samples;
    const size_t L     = (bytes.size() + (SAMPLE_BYTES - 1)) / SAMPLE_BYTES; // whole samples (ceil)

    cpp_int index = 0;
    if (L != 0) {
        // Big-endian-by-sample payload bytes (V), most-significant-sample first
        // for import_bits(msv=true). S_L (the repunit) comes from the shared
        // helper in Utilities.h.
        std::vector<uint8_t> valueBytes(L * SAMPLE_BYTES, 0);
        for (size_t i = 0; i < L; ++i) {
            size_t   lo  = i * SAMPLE_BYTES;
            uint32_t low = bytes[lo];
            // A stray trailing byte (should not occur for 16-bit data) is treated
            // as a low byte with a zero high byte so no value is silently dropped.
            uint32_t high = (lo + 1 < bytes.size()) ? bytes[lo + 1] : 0U;

            // Sample value, big-endian into valueBytes (high byte first).
            valueBytes[lo]     = static_cast<uint8_t>(high);
            valueBytes[lo + 1] = static_cast<uint8_t>(low);
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
    /**
     * Payload-only bijection (integer -> samples), O(N) closed form.
     *
     * Inverse of encode via the same identity n = V + S_L. Every alphabet-valid
     * index decodes; nothing is rejected, and there is intentionally no
     * integrity check.
     *
     * The sample count L is recovered without bignum division: for an L-sample
     * payload, n lies in [S_L, S_{L+1}-1], and with m = n*(B-1) + 1,
     *   L = floor(log_B(m)) = msb(m) / log2(B) = msb(m) / 16.
     * Then S_L is the repunit, V = n - S_L (V < B^L), and the L base-B digits
     * of V are the samples (most significant first). All steps are O(N).
     *
     * Decoded samples are serialized little-endian; see FileIO for wrapping
     * the payload with a WAV header.
     */
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

        // Each sample is big-endian [high, low] in `padded`; emit little-endian.
        samples.resize(L * SAMPLE_BYTES);
        for (size_t i = 0; i < L; ++i) {
            size_t off       = i * SAMPLE_BYTES;
            samples[off]     = padded[off + 1]; // low byte
            samples[off + 1] = padded[off];     // high byte
        }
    }

    return samples;
}

} // namespace AudioBabel
