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
        auto chunkSize = read_le<uint32_t>(sizeBuf.data());
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

// Debug storage for import/export statistics (thread_local to avoid data races)
static thread_local AudioIndex::DebugInfo lastDebug;

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
     * Build index with 13-byte header + PCM samples
     * Header format (little-endian):
     *   Byte 0:      VERSION (0x01)
     *   Byte 1-4:    num_frames (uint32_t)
     *   Byte 5-8:    sample_rate (uint32_t)
     *   Byte 9-10:   bit_depth (uint16_t)
     *   Byte 11-12:  num_channels (uint16_t)
     *   Byte 13+:    PCM sample data
     */
    auto t0_import = std::chrono::steady_clock::now();

    std::vector<uint8_t> index_bytes;
    size_t               bytes_per_sample = audioData.bit_rate / BITS_PER_BYTE;
    size_t               total_samples    = audioData.num_frames * audioData.num_channels;

    // Reserve space for header + PCM data
    index_bytes.reserve(13 + (total_samples * bytes_per_sample));

    // Write 13-byte header
    index_bytes.push_back(0x01); // VERSION byte

    // num_frames (4 bytes, little-endian)
    auto num_frames_u32 = static_cast<uint32_t>(audioData.num_frames);
    Utilities::push_le<uint32_t>(index_bytes, num_frames_u32);

    // sample_rate (4 bytes, little-endian)
    Utilities::push_le<uint32_t>(index_bytes, audioData.sample_rate);

    // bit_depth (2 bytes, little-endian)
    Utilities::push_le<uint16_t>(index_bytes, audioData.bit_rate);

    // num_channels (2 bytes, little-endian)
    Utilities::push_le<uint16_t>(index_bytes, audioData.num_channels);

    // Append PCM sample data (already in little-endian byte order)
    index_bytes.insert(index_bytes.end(), audioData.samples.begin(), audioData.samples.end());

    // Convert entire byte array (header + PCM) to cpp_int (MSB-first)
    cpp_int index = 0;
    boost::multiprecision::import_bits(index, index_bytes.begin(), index_bytes.end(), BITS_PER_BYTE, true);

    // Populate debug info
    lastDebug.import_pcm_bytes      = index_bytes.size();
    lastDebug.import_expected_bytes = 13 + (total_samples * bytes_per_sample);

    auto t1_import               = std::chrono::steady_clock::now();
    lastDebug.audioDataToIndexMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1_import - t0_import).count());

    return index;
}

auto AudioIndex::indexToAudioData(const boost::multiprecision::cpp_int& index) -> AudioIndex::AudioData {
    // Export index to bytes (MSB-first)
    std::vector<uint8_t> bytes;
    boost::multiprecision::export_bits(index, std::back_inserter(bytes), BITS_PER_BYTE, true);

    // Validate minimum size (13 bytes header + at least 0 bytes PCM)
    if (bytes.size() < 13) {
        throw std::runtime_error("Invalid index: too small to contain header (expected at least 13 bytes, got " + std::to_string(bytes.size()) + ")");
    }

    // Validate version byte
    if (bytes[0] != 0x01) {
        throw std::runtime_error("Invalid index: unsupported version byte 0x" + std::to_string(static_cast<int>(bytes[0])) + " (expected 0x01)");
    }

    // Read header fields (little-endian)
    auto num_frames   = Utilities::read_le<uint32_t>(reinterpret_cast<const char*>(&bytes[1]));
    auto sample_rate  = Utilities::read_le<uint32_t>(reinterpret_cast<const char*>(&bytes[5]));
    auto bit_depth    = Utilities::read_le<uint16_t>(reinterpret_cast<const char*>(&bytes[9]));
    auto num_channels = Utilities::read_le<uint16_t>(reinterpret_cast<const char*>(&bytes[11]));

    // Validate bit depth
    if (!isBitDepthSupported(bit_depth)) {
        std::ostringstream ss;
        ss << "Unsupported bit depth in index header: " << bit_depth;
        throw std::runtime_error(ss.str());
    }

    // Validate header consistency with PCM data size
    size_t bytes_per_sample   = bit_depth / BITS_PER_BYTE;
    size_t expected_pcm_bytes = static_cast<size_t>(num_frames) * static_cast<size_t>(num_channels) * bytes_per_sample;
    size_t actual_pcm_bytes   = bytes.size() - 13; // 13-byte header

    // Validate PCM payload size
    // Throw on excess: more PCM bytes than header declares means genuine corruption.
    // Deficit is expected: export_bits strips trailing zero bytes from the big
    // integer (the PCM data sits in the low-order bits), so we pad zeros back.
    if (actual_pcm_bytes > expected_pcm_bytes) {
        std::ostringstream ss;
        ss << "Index PCM size mismatch: header expects " << expected_pcm_bytes << " bytes but found " << actual_pcm_bytes << " bytes";
        throw std::runtime_error(ss.str());
    } else if (actual_pcm_bytes < expected_pcm_bytes) {
        // Trailing zero bytes were stripped by export_bits — restore them.
        size_t bytes_to_add = expected_pcm_bytes - actual_pcm_bytes;
        bytes.insert(bytes.end(), bytes_to_add, 0);
    }

    // Extract PCM samples (bytes 13 onward, already in little-endian per-sample format)
    AudioData audioData{};
    audioData.audio_format = PCM_FORMAT_CODE; // PCM = 1
    audioData.num_channels = num_channels;
    audioData.sample_rate  = sample_rate;
    audioData.bit_rate     = bit_depth;
    audioData.num_frames   = num_frames;

    // Copy PCM data (skip the 13-byte header)
    audioData.samples.assign(bytes.begin() + 13, bytes.begin() + 13 + expected_pcm_bytes);

    // Record export stats
    lastDebug.export_pcm_bytes      = actual_pcm_bytes;
    lastDebug.export_expected_bytes = expected_pcm_bytes;

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
