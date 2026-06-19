#include "AudioIndex.h"

#include <algorithm>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "Constants.h"
#include "FileWriters.h"
#include "IndexScramble.h"
#include "Utilities.h"

using boost::multiprecision::cpp_int;
namespace fs = std::filesystem;

namespace AudioBabel {

// ---------------------------------------------------------------------------
// 1) Binary IO helpers
// ---------------------------------------------------------------------------

using namespace Utilities;

/**
 * We currently only support Bit Rates/Bit Depths of 8, 16, and 32
 */
auto isBitDepthSupported(uint16_t bitDepth) -> bool {
    return PCM_BITS_PER_SAMPLE.end() != std::find(PCM_BITS_PER_SAMPLE.begin(), PCM_BITS_PER_SAMPLE.end(), bitDepth);
}

// ---------------------------------------------------------------------------
// 2) Factory functions
// ---------------------------------------------------------------------------

auto AudioIndex::extractAudioDataFromAudioFile(const std::string& path) -> AudioIndex::AudioData {
    // Extracts PCM audio data from a WAV file's RIFF/WAVE chunk layout. Only PCM is supported.

    std::ifstream fileInput(path, std::ios::binary);
    if (!fileInput) {
        throw std::runtime_error(std::string("Failed to open WAV file: ") + path);
    }

    std::array<char, WAV_ID_LEN> riff{};
    fileInput.read(riff.data(), WAV_ID_LEN);
    if (!tagEquals(riff, "RIFF")) {
        std::string        found(riff.data(), WAV_ID_LEN);
        std::ostringstream ss;
        ss << "Not a RIFF file: header='" << found << "'";
        throw std::runtime_error(ss.str());
    }

    std::array<char, WAV_ID_LEN> tmp4{}; // file size field, unused
    fileInput.read(tmp4.data(), WAV_ID_LEN);

    std::array<char, WAV_ID_LEN> wave{};
    fileInput.read(wave.data(), WAV_ID_LEN);
    if (!tagEquals(wave, "WAVE")) {
        std::string        found(wave.data(), WAV_ID_LEN);
        std::ostringstream ss;
        ss << "Not a WAVE file: header='" << found << "'";
        throw std::runtime_error(ss.str());
    }

    AudioData audioData{};
    while (fileInput) {
        std::array<char, WAV_ID_LEN> id{};
        if (!fileInput.read(id.data(), WAV_ID_LEN)) {
            break;
        }

        std::array<char, WAV_ID_LEN> sizeBuf{};
        if (!fileInput.read(sizeBuf.data(), WAV_ID_LEN)) {
            break;
        }

        // Interpret chunk size, guard against unreasonable chunk sizes (> 1 GiB or 1,073,741,824 bytes)
        auto chunkSize = read_le<uint32_t>(sizeBuf.data());
        if (chunkSize > ::AudioBabel::CHUNK_SIZE_LIMIT) {
            break;
        }

        // "fmt " (trailing space intentional) requires at least 16 bytes for PCM
        if (tagEquals(id, "fmt ")) {
            if (chunkSize < FMT_CHUNK_MIN_SIZE) {
                break;
            }

            std::vector<char> buf(chunkSize);
            if (!fileInput.read(buf.data(), chunkSize)) {
                break;
            }

            audioData.audio_format = read_le<uint16_t>(buf.data());
            audioData.num_channels = read_le<uint16_t>(buf.data() + 2);
            audioData.sample_rate  = read_le<uint32_t>(buf.data() + 4);
            audioData.bit_rate     = read_le<uint16_t>(buf.data() + 14);

            // Validate bit depth read from fmt chunk to avoid later division by zero
            if (!isBitDepthSupported(audioData.bit_rate)) {
                std::ostringstream ss;
                ss << "Unsupported bits per sample in WAV fmt chunk: bitsPerSample=" << audioData.bit_rate << " sample_rate=" << audioData.sample_rate
                   << " num_channels=" << audioData.num_channels;
                throw std::runtime_error(ss.str());
            }
        }

        else if (tagEquals(id, "data")) {
            audioData.samples.resize(chunkSize);
            fileInput.read(reinterpret_cast<char*>(audioData.samples.data()), static_cast<std::streamsize>(chunkSize));

            // If we didn't get the full declared chunk, this indicates the file is truncated
            // (declared size > actual bytes available). Treat this as a fatal error.
            std::streamsize bytesRead = fileInput.gcount();
            if (static_cast<uint32_t>(bytesRead) != chunkSize) {
                std::ostringstream ss;
                ss << "Declared data chunk larger than actual bytes available: declared=" << chunkSize << " read=" << bytesRead << " path=" << path;
                throw std::runtime_error(ss.str());
            }
        }

        // Unknown chunk: skip it, plus one pad byte if its size is odd (RIFF chunks are even-sized).
        else {
            fileInput.seekg(chunkSize + (chunkSize & 1), std::ios::cur);
            if (!fileInput) {
                break;
            }
        }
    }

    // Validate that we found a data chunk and populated samples
    if (audioData.samples.empty()) {
        std::ostringstream ss;
        ss << "No data chunk found in WAV: path=" << path;
        throw std::runtime_error(ss.str());
    }

    // Compute number of frames
    // Number of Frames = Total Samples / ((Bit Rate / 8) * Number of Channels)
    // Defensive checks: ensure bit_rate and num_channels are reasonable to avoid division by zero
    if (audioData.bit_rate == 0 || audioData.num_channels == 0) {
        std::ostringstream ss;
        ss << "Invalid WAV header fields: bit_rate=" << audioData.bit_rate << " num_channels=" << audioData.num_channels << " path=" << path;
        throw std::runtime_error(ss.str());
    }
    // bit_rate is necessarily one of PCM_BITS_PER_SAMPLE here: the only place it's set
    // (the "fmt " chunk branch above) already validates it via isBitDepthSupported, and
    // a zero bit_rate (unset) was just rejected above.
    size_t bytes_per_sample = audioData.bit_rate / BITS_PER_BYTE;
    audioData.num_frames    = audioData.samples.size() / (bytes_per_sample * audioData.num_channels);
    return audioData;
}

auto AudioIndex::extractAudioDataFromSamples(const std::vector<int32_t>& samples, int sampleRate, int bitDepth) -> AudioIndex::AudioData {
    AudioData audioData{};
    audioData.sample_rate  = static_cast<uint32_t>(sampleRate);
    audioData.bit_rate     = static_cast<uint16_t>(bitDepth);
    audioData.num_channels = DEFAULT_NUM_CHANNELS; // assuming mono input
    audioData.audio_format = PCM_FORMAT_CODE;      // PCM format

    size_t bytes_per_sample = bitDepth / BITS_PER_BYTE;
    audioData.samples.resize(samples.size() * bytes_per_sample);

    for (size_t sampleIndex = 0; sampleIndex < samples.size(); ++sampleIndex) {
        int32_t sample = samples[sampleIndex];
        for (size_t byteIndex = 0; byteIndex < bytes_per_sample; ++byteIndex) {
            audioData.samples[(sampleIndex * bytes_per_sample) + byteIndex] =
                static_cast<uint8_t>((sample >> (byteIndex * BITS_PER_BYTE)) & BYTE_MASK);
        }
    }

    audioData.num_frames = samples.size() / audioData.num_channels;
    return audioData;
}

// Bytes per PCM sample at the default bit depth (2 for 16-bit).
static constexpr size_t SAMPLE_BYTES = DEFAULT_BIT_DEPTH / BITS_PER_BYTE;

auto AudioIndex::audioDataToIndex(const AudioIndex::AudioData& audioData) -> boost::multiprecision::cpp_int {
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
    const auto&  bytes = audioData.samples;
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
    index = IndexScramble::applyScramble(index);

    return index;
}

auto AudioIndex::indexToAudioData(const boost::multiprecision::cpp_int& index) -> AudioIndex::AudioData {
    /**
     * Payload-only bijection (integer -> samples), O(N) closed form.
     *
     * Inverse of audioDataToIndex via the same identity n = V + S_L. Every
     * alphabet-valid index decodes; nothing is rejected, and there is
     * intentionally no integrity check.
     *
     * The sample count L is recovered without bignum division: for an L-sample
     * payload, n lies in [S_L, S_{L+1}-1], and with m = n*(B-1) + 1,
     *   L = floor(log_B(m)) = msb(m) / log2(B) = msb(m) / 16.
     * Then S_L is the repunit, V = n - S_L (V < B^L), and the L base-B digits
     * of V are the samples (most significant first). All steps are O(N).
     *
     * Decoded samples are serialized little-endian and wrapped in a fixed
     * default header (PCM, 44100 Hz, 16-bit, mono) for WAV writing.
     */
    // Undo the optional reversible scramble (identity unless enabled) before
    // decoding. The stored index is what carries the scramble.
    const cpp_int idx = IndexScramble::applyUnscramble(index);

    AudioData audioData{};
    audioData.audio_format = PCM_FORMAT_CODE;
    audioData.num_channels = DEFAULT_NUM_CHANNELS;
    audioData.sample_rate  = DEFAULT_SAMPLE_RATE;
    audioData.bit_rate     = DEFAULT_BIT_DEPTH;

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
        audioData.samples.resize(L * SAMPLE_BYTES);
        for (size_t i = 0; i < L; ++i) {
            size_t off                 = i * SAMPLE_BYTES;
            audioData.samples[off]     = padded[off + 1]; // low byte
            audioData.samples[off + 1] = padded[off];     // high byte
        }
    }

    audioData.num_frames = (audioData.samples.size() / SAMPLE_BYTES) / audioData.num_channels;

    return audioData;
}

} // namespace AudioBabel
