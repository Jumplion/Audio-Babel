#include "AudioIndex.h"

#include <algorithm>
#include <boost/multiprecision/cpp_int.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "Constants.h"
#include "FileWriters.h"
#include "IndexMetadata.h"
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
// 2) Operators
// ---------------------------------------------------------------------------

auto AudioIndex::operator=(const AudioIndex& other) -> AudioIndex& {
    if (this != &other) {
        audioData = other.audioData;
    }
    return *this;
}

auto AudioIndex::operator==(const AudioIndex& other) const -> bool {
    return audioData.audio_format == other.audioData.audio_format && audioData.num_channels == other.audioData.num_channels &&
           audioData.sample_rate == other.audioData.sample_rate && audioData.bit_rate == other.audioData.bit_rate &&
           audioData.num_frames == other.audioData.num_frames && audioData.samples == other.audioData.samples;
}

auto AudioIndex::operator!=(const AudioIndex& other) const -> bool {
    return !(*this == other);
}

// ---------------------------------------------------------------------------
// 3) Factory functions
// ---------------------------------------------------------------------------

auto AudioIndex::fromAudioSamples(const std::vector<int32_t>& samples, int sampleRate, int bitDepth) -> AudioIndex {
    AudioIndex index;
    index.audioData = AudioIndex::extractAudioDataFromSamples(samples, sampleRate, bitDepth);
    // build index integer and derive metadata
    try {
        cpp_int idx    = AudioIndex::audioDataToIndex(index.audioData);
        index.metadata = IndexMetadata::extractMetadataFromIndex(idx);
    } catch (...) {
        // non-fatal: leave metadata blank on error
    }
    return index;
}

auto AudioIndex::extractAudioDataFromAudioFile(const std::string& path) -> AudioIndex::AudioData {
    /**
     * EXPLANATION FOR EXTRACTION ALGORITHM
     * This function extracts audio data from a WAV file by reading its headers
     * and data chunks. It supports only PCM format and assumes a specific chunk
     * layout.
     *
     * A .wav file consists of a header and a data chunk. The header contains
     * metadata about the audio format, while the data chunk contains the actual
     * PCM audio samples.
     *
     * The header is 44 bytes long and contains the following fields:
     * - ChunkID (4 bytes): Contains the letters "RIFF" in ASCII form.
     * - ChunkSize (4 bytes): 36 + SubChunk2Size, or more generally: 4 + (8 + SubChunk2Size)
     * - Format (4 bytes): Contains the letters "WAVE".
     * - Subchunk1ID (4 bytes): Contains the letters "fmt ".
     * - Subchunk1Size (4 bytes): 16 for PCM.
     * - AudioFormat (2 bytes): PCM = 1.
     * - NumChannels (2 bytes): Mono = 1, Stereo = 2. (NOTE: We Assume 1)
     * - SampleRate (4 bytes): 8000, 44100, etc.    (NOTE: We assume 44100)
     * - ByteRate (4 bytes): SampleRate * NumChannels * BitsPerSample/8.
     * - BlockAlign (2 bytes): NumChannels * BitsPerSample/8.
     * - BitsPerSample (2 bytes): 8 bits = 8, 16 bits = 16. (NOTE: We Assume 16)
     *
     * The data chunk contains the actual PCM audio samples.
     */

    // Open the WAV file for reading
    std::ifstream fileInput(path, std::ios::binary);
    if (!fileInput) {
        throw std::runtime_error(std::string("Failed to open WAV file: ") + path);
    }

    // Read RIFF header, throw error if not found
    std::array<char, WAV_ID_LEN> riff{};
    fileInput.read(riff.data(), WAV_ID_LEN);
    if (!tagEquals(riff, "RIFF")) {
        std::string        found(riff.data(), WAV_ID_LEN);
        std::ostringstream ss;
        ss << "Not a RIFF file: header='" << found << "'";
        throw std::runtime_error(ss.str());
    }

    // Read file size
    std::array<char, WAV_ID_LEN> tmp4{};
    fileInput.read(tmp4.data(), WAV_ID_LEN);

    // Read "WAVE" header, throw error if not found
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
        /**
         * Each iteration reads a 4 bytes into a char[4] and then reads the next 4 bytes into a
         * temporary buffer sizeBuf. The code converts those four bytes to a uint32_t.
         * This explicitly interprets the on‑disk size as LITTLE-ENDIAN
         *     - (see: Endianness [https://en.wikipedia.org/wiki/Endianness]) -
         * which is why the size is read then converted, rather than read directly into a uint32_t.
         */
        // Read chunk ID
        std::array<char, WAV_ID_LEN> id{};
        if (!fileInput.read(id.data(), WAV_ID_LEN)) {
            break;
        }

        std::array<char, WAV_ID_LEN> sizeBuf{};
        if (!fileInput.read(sizeBuf.data(), WAV_ID_LEN)) {
            break;
        }

        // Interpret chunk size, guard against unreasonable chunk sizes (> 1 GiB or 1,073,741,824 bytes)
        uint32_t chunkSize = read_u32_le(sizeBuf.data());
        if (chunkSize > ::AudioBabel::CHUNK_SIZE_LIMIT) {
            break;
        }

        /**
         * Process "fmt " chunk. Requires at least 16 bytes (minimum for PCM fmt).
         *     (NOTE: the space at the end is intentional)
         *     Reads the entire chunk into a temporary buffer, then extracts
         *     fields using the provided little‑endian helpers:
         *     - audio_format at offset 0
         *     - num_channels at offset 2
         *     - sample_rate at offset 4
         *     - bits_per_sample at offset 14
         *         NOTE: These offsets correspond to the standard 16‑byte PCM fmt layout:
         *             AudioFormat, NumChannels, SampleRate, ByteRate, BlockAlign, BitsPerSample
         */
        if (tagEquals(id, "fmt ")) {
            if (chunkSize < FMT_CHUNK_MIN_SIZE) {
                break;
            }

            std::vector<char> buf(chunkSize);
            if (!fileInput.read(buf.data(), chunkSize)) {
                break;
            }

            audioData.audio_format = read_u16_le(buf.data());
            audioData.num_channels = read_u16_le(buf.data() + 2);
            audioData.sample_rate  = read_u32_le(buf.data() + 4);
            audioData.bit_rate     = read_u16_le(buf.data() + 14);

            // Validate bit depth read from fmt chunk to avoid later division by zero
            if (!isBitDepthSupported(audioData.bit_rate)) {
                std::ostringstream ss;
                ss << "Unsupported bits per sample in WAV fmt chunk: bitsPerSample=" << audioData.bit_rate << " sample_rate=" << audioData.sample_rate
                   << " num_channels=" << audioData.num_channels;
                throw std::runtime_error(ss.str());
            }
        }

        /**
         *  Process "data" chunk (the samples)
         *  Resizes audioData.samples to the chunk size and 
         *  reads raw sample bytes into that buffer.
         */
        else if (tagEquals(id, "data")) {
            // Reserve space for the declared data chunk then attempt to read it.
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

        /**
         * We have an unknown chunk. 
         * Skip this chunk and account for RIFF padding (chunks are even-sized) by
         * advancing the stream by sz bytes plus one extra byte when sz is odd (sz + (sz & 1)),
         */
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
    if (!isBitDepthSupported(audioData.bit_rate)) {
        std::ostringstream ss;
        ss << "Unsupported bits per sample in WAV: " << audioData.bit_rate << " path=" << path;
        throw std::runtime_error(ss.str());
    }
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

    // Convert int32 samples to bytes (little-endian)
    size_t bytes_per_sample = bitDepth / BITS_PER_BYTE;
    audioData.samples.resize(samples.size() * bytes_per_sample);

    // Loop through samples.
    for (size_t sampleIndex = 0; sampleIndex < samples.size(); ++sampleIndex) {
        int32_t sample = samples[sampleIndex];

        // Convert this sample to
        for (size_t byteIndex = 0; byteIndex < bytes_per_sample; ++byteIndex) {
            audioData.samples[(sampleIndex * bytes_per_sample) + byteIndex] =
                static_cast<uint8_t>((sample >> (byteIndex * BITS_PER_BYTE)) & BYTE_MASK);
        }
    }

    // Compute number of frames
    audioData.num_frames = samples.size() / audioData.num_channels;
    return audioData;
}

// Debug storage for import/export statistics
static AudioIndex::DebugInfo lastDebug;

auto AudioIndex::getLastDebugInfo() -> AudioIndex::DebugInfo {
    return lastDebug;
}

void AudioIndex::clearLastDebugInfo() {
    lastDebug = DebugInfo();
}

auto AudioIndex::audioDataToIndex(const AudioIndex::AudioData& audioData) -> boost::multiprecision::cpp_int {
    if (!isBitDepthSupported(audioData.bit_rate)) {
        std::ostringstream ss;
        ss << "Unsupported bit depth in audioDataToIndex: " << audioData.bit_rate << " sample_rate=" << audioData.sample_rate
           << " num_channels=" << audioData.num_channels;
        throw std::runtime_error(ss.str());
    }

    /**
     * Build pcm_int (our index) by concatenating samples and appending the header.
     * The byte layout is assumed as follows:
     *      Index = [PCM_payload (Samples)] [16-byte Header]
     *
     * To construct the PCM portion efficiently we assemble a MSB-first byte
     * buffer and call boost::multiprecision::import_bits with the MSB flag set.
    */
    size_t  bytes_per_sample = audioData.bit_rate / BITS_PER_BYTE;
    size_t  total_samples    = audioData.num_frames * audioData.num_channels;
    cpp_int pcm_int          = 0;

    auto t0_import = std::chrono::steady_clock::now();
    if (total_samples > 0) {
        std::vector<uint8_t> pcm_be;
        pcm_be.reserve(total_samples * bytes_per_sample);

        // Convert samples from little-endian per-sample bytes to big-endian byte stream
        for (size_t sampleIndex = 0; sampleIndex < total_samples; ++sampleIndex) {
            size_t offset = sampleIndex * bytes_per_sample;
            append_sample_be_from_le(audioData.samples, offset, bytes_per_sample, pcm_be);
        }

        // Import bytes into cpp_int (MSB-first)
        boost::multiprecision::import_bits(pcm_int, pcm_be.begin(), pcm_be.end(), BITS_PER_BYTE, true);

        // populate debug info
        lastDebug.import_pcm_bytes      = pcm_be.size();
        lastDebug.import_expected_bytes = total_samples * bytes_per_sample;
    }

    auto t1_import               = std::chrono::steady_clock::now();
    lastDebug.audioDataToIndexMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1_import - t0_import).count());

    // Build explicit header bytes (big-endian):
    //      u32 sample_rate, u16 bit_depth, u16 num_channels, u64 num_frames
    // The header is appended at the end (into the least-significant HEADER_BYTES_CONST bytes) so
    // we can extract it by masking the low bits.
    std::vector<uint8_t> header_buf;
    header_buf.reserve(HEADER_BYTES_CONST);

    push_be_u32(header_buf, audioData.sample_rate);
    push_be_u16(header_buf, audioData.bit_rate);
    push_be_u16(header_buf, audioData.num_channels);
    push_be_u64(header_buf, audioData.num_frames);

    // Convert header_buf to cpp_int (big-endian bytes)
    cpp_int header_int = bytes_to_cpp_int_be(header_buf);

    // Combine: shift pcm_int left by header_bits and OR header_int
    size_t  header_bits = HEADER_BYTES_CONST * BITS_PER_BYTE;
    cpp_int idx         = (pcm_int << header_bits) | header_int;
    return idx;
}

auto AudioIndex::indexToAudioData(const boost::multiprecision::cpp_int& index) -> AudioIndex::AudioData {
    const size_t HEADER_BITS = HEADER_BYTES_CONST * BITS_PER_BYTE;

    // Extract header_int from the last HEADER_BITS of the index
    cpp_int mask       = (cpp_int(1) << HEADER_BITS) - 1;
    cpp_int header_int = index & mask;
    cpp_int pcm_int    = index >> HEADER_BITS;

    // Convert header to bytes (big-endian)
    std::vector<uint8_t> header_buf(HEADER_BYTES_CONST);
    cpp_int              tmp = header_int;
    for (int headerIndex = static_cast<int>(HEADER_BYTES_CONST) - 1; headerIndex >= 0; --headerIndex) {
        uint8_t byte            = static_cast<uint8_t>(static_cast<uint64_t>(tmp & BYTE_MASK));
        header_buf[headerIndex] = byte;
        tmp >>= BITS_PER_BYTE;
    }

    // Parse fields from header_buf (big-endian)
    uint32_t sample_rate = (static_cast<uint32_t>(header_buf[0]) << 24) | (static_cast<uint32_t>(header_buf[1]) << 16) |
                           (static_cast<uint32_t>(header_buf[2]) << 8) | static_cast<uint32_t>(header_buf[3]);
    auto     bit_depth    = static_cast<uint16_t>((static_cast<uint16_t>(header_buf[4]) << 8) | static_cast<uint16_t>(header_buf[5]));
    auto     num_channels = static_cast<uint16_t>((static_cast<uint16_t>(header_buf[6]) << 8) | static_cast<uint16_t>(header_buf[7]));
    uint64_t num_frames   = 0;
    for (int i = 0; i < 8; ++i) {
        num_frames = (num_frames << BITS_PER_BYTE) | header_buf[8 + i];
    }

    if (!isBitDepthSupported(bit_depth)) {
        std::ostringstream ss;
        ss << "Unsupported bit depth in indexToAudioData: " << bit_depth << " sample_rate=" << sample_rate << " num_channels=" << num_channels
           << " num_frames=" << num_frames;
        throw std::runtime_error(ss.str());
    }

    // Compute number of samples
    size_t                bytes_per_sample = bit_depth / BITS_PER_BYTE;
    size_t                total_samples    = static_cast<size_t>(num_frames) * static_cast<size_t>(num_channels);
    cpp_int               sample_mask      = (cpp_int(1) << bit_depth) - 1;
    std::vector<uint64_t> samples;
    samples.reserve(total_samples);

    // Attempt to infer num_frames from the index payload when header num_frames is zero
    std::vector<uint8_t> pcm_be_bytes;
    if (total_samples == 0) {
        // Export all available PCM bytes (MSB-first). If the index contains payload
        // bytes, we can derive total_samples and num_frames from the payload length.
        boost::multiprecision::export_bits(pcm_int, std::back_inserter(pcm_be_bytes), BITS_PER_BYTE, true);
        if (!pcm_be_bytes.empty() && bytes_per_sample > 0) {
            // derive total_samples from available bytes (floor division)
            total_samples = pcm_be_bytes.size() / bytes_per_sample;
            num_frames    = (total_samples / num_channels);
        }
    }

    // Extract samples: use export_bits to extract PCM bytes in big-endian
    if (total_samples > 0) {
        if (pcm_be_bytes.empty()) {
            // export_bits writes least-significant byte first by default; request MSB-first
            pcm_be_bytes.reserve(total_samples * bytes_per_sample);
            boost::multiprecision::export_bits(pcm_int, std::back_inserter(pcm_be_bytes), BITS_PER_BYTE, true);
        }

        // pcm_be_bytes now contains samples in big-endian sample order
        // We need to split into samples and convert each to host-endian little-endian byte order
        size_t expected_bytes = total_samples * bytes_per_sample;
        if (pcm_be_bytes.size() != expected_bytes) {
            if (pcm_be_bytes.size() < expected_bytes) {
                // export_bits may omit leading zero bytes; pad at the front (MSB side)
                size_t pad = expected_bytes - pcm_be_bytes.size();
                pcm_be_bytes.insert(pcm_be_bytes.begin(), pad, 0);
            } else {
                // If larger, keep the least-significant expected bytes (rightmost)
                pcm_be_bytes = std::vector<uint8_t>(pcm_be_bytes.end() - expected_bytes, pcm_be_bytes.end());
            }
        }

        // iterate samples in order and convert each to unsigned sample words
        for (size_t sampleIndex = 0; sampleIndex < total_samples; ++sampleIndex) {
            size_t   base = sampleIndex * bytes_per_sample;
            uint64_t word = 0;
            for (size_t byteIndex = 0; byteIndex < bytes_per_sample; ++byteIndex) {
                word = (word << BITS_PER_BYTE) | static_cast<uint64_t>(pcm_be_bytes[base + byteIndex]);
            }
            // Handle signed values depending on bit depth
            uint64_t signbit = static_cast<uint64_t>(1) << (bit_depth - 1);
            int64_t  sval    = 0;
            if ((word & signbit) != 0U) {
                sval = static_cast<int64_t>(word - (static_cast<uint64_t>(1) << bit_depth));
            } else {
                sval = static_cast<int64_t>(word);
            }
            samples.push_back(static_cast<uint64_t>(sval & ((1ULL << bit_depth) - 1)));
        }

        // record export stats
        lastDebug.export_pcm_bytes      = pcm_be_bytes.size();
        lastDebug.export_expected_bytes = expected_bytes;
    }

    // pack bytes
    AudioData audioData{};
    audioData.audio_format = 1;
    audioData.num_channels = num_channels;
    audioData.sample_rate  = sample_rate;
    audioData.bit_rate     = bit_depth;
    // reuse bytes_per_sample computed above
    audioData.samples.resize(total_samples * bytes_per_sample);

    for (size_t sampleIndex = 0; sampleIndex < total_samples; sampleIndex++) {
        uint64_t v = samples[sampleIndex];
        for (size_t byteIndex = 0; byteIndex < bytes_per_sample; byteIndex++) {
            audioData.samples[(sampleIndex * bytes_per_sample) + byteIndex] = static_cast<uint8_t>((v >> (byteIndex * BITS_PER_BYTE)) & BYTE_MASK);
        }
    }
    audioData.num_frames = static_cast<size_t>(num_frames);
    return audioData;
}

auto AudioIndex::indexToMetadata(const boost::multiprecision::cpp_int& index) -> IndexMetadata {
    return IndexMetadata::extractMetadataFromIndex(index);
}

void AudioIndex::exportAudioDataToWav(const AudioData& audioData, const std::string& path) {
    FileWriters::exportAudioDataToWav(audioData, path);
}

void AudioIndex::writeIndexToFile(const boost::multiprecision::cpp_int& index, const std::string& outDir, const std::string& filename) {
    FileWriters::writeIndexToFile(index, outDir, filename);
}

} // namespace AudioBabel
