#include "FileIO.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "Constants.h"
#include "Utilities.h"

using boost::multiprecision::cpp_int;
namespace fs = std::filesystem;

namespace AudioBabel {

using namespace Utilities;

/**
 * We currently only support Bit Rates/Bit Depths of 8, 16, and 32
 */
static auto isBitDepthSupported(uint16_t bitDepth) -> bool {
    return PCM_BITS_PER_SAMPLE.end() != std::find(PCM_BITS_PER_SAMPLE.begin(), PCM_BITS_PER_SAMPLE.end(), bitDepth);
}

auto FileIO::readWav(const std::string& path) -> FileIO::AudioData {
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

void FileIO::writeWav(const FileIO::AudioData& audioData, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open output WAV: " + path);
    }

    out.write("RIFF", 4);
    uint32_t file_size = WAV_FILE_BASE_OVERHEAD + static_cast<uint32_t>(audioData.samples.size());
    write_le<uint32_t>(out, file_size);
    out.write("WAVE", 4);

    out.write("fmt ", 4);
    auto fmt_size = static_cast<uint32_t>(FMT_CHUNK_MIN_SIZE);
    write_le<uint32_t>(out, fmt_size);

    uint16_t audio_format = audioData.audio_format;
    write_le<uint16_t>(out, audio_format);
    write_le<uint16_t>(out, audioData.num_channels);
    write_le<uint32_t>(out, audioData.sample_rate);

    uint32_t byte_rate = audioData.sample_rate * audioData.num_channels * (audioData.bit_rate / 8);
    write_le<uint32_t>(out, byte_rate);

    auto block_align = static_cast<uint16_t>(audioData.num_channels * (audioData.bit_rate / 8));
    write_le<uint16_t>(out, block_align);
    write_le<uint16_t>(out, audioData.bit_rate);

    out.write("data", 4);
    auto data_size = static_cast<uint32_t>(audioData.samples.size());
    write_le<uint32_t>(out, data_size);
    out.write(reinterpret_cast<const char*>(audioData.samples.data()), audioData.samples.size());
}

void FileIO::writeWav(const std::vector<uint8_t>& samples, const std::string& path) {
    AudioData audioData{};
    audioData.audio_format = PCM_FORMAT_CODE;
    audioData.num_channels = DEFAULT_NUM_CHANNELS;
    audioData.sample_rate  = DEFAULT_SAMPLE_RATE;
    audioData.bit_rate     = DEFAULT_BIT_DEPTH;
    audioData.samples      = samples;
    audioData.num_frames   = (samples.size() / (DEFAULT_BIT_DEPTH / BITS_PER_BYTE)) / DEFAULT_NUM_CHANNELS;

    writeWav(audioData, path);
}

auto FileIO::fromSamples(const std::vector<int32_t>& samples, int sampleRate, int bitDepth) -> FileIO::AudioData {
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

void FileIO::writeIndexToFile(const boost::multiprecision::cpp_int& index, const std::string& outDir, const std::string& filename) {
    // Determine directory to write into.
    fs::path dir;
    if (outDir.empty()) {
        dir = fs::path("cpp") / "tests" / "indexes";
    } else {
        dir = fs::path(outDir);
    }

    try {
        fs::create_directories(dir);
    } catch (...) {
        // best-effort
    }

    // Serialize the index through the bijective base-64 encoding (single string
    // encoding shared across the whole system).
    std::string base64Str = Utilities::indexToB64(index);

    // Make a short stable stem from the leading characters of the index string.
    // The empty index (integer 0) yields an empty string, so fall back to "0".
    std::string stem = base64Str.empty() ? std::string("0") : base64Str.substr(0, std::min<size_t>(base64Str.size(), 12));

    // choose base name: provided filename or generated stem
    std::string name = filename.empty() ? stem : filename;

    std::ofstream out((dir / (name + ".txt")).string());
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + (dir / (name + ".txt")).string());
    }

    out << base64Str << '\n';
    out.close();
}

} // namespace AudioBabel
