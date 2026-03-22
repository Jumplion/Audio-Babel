#include "FileWriters.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "Utilities.h"

using boost::multiprecision::cpp_int;
namespace fs = std::filesystem;

namespace AudioBabel {

using namespace Utilities;

void FileWriters::exportAudioDataToWav(const AudioIndex::AudioData& audioData, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open output WAV: " + path);
    }

    // RIFF header
    out.write("RIFF", 4);
    uint32_t file_size = 36 + static_cast<uint32_t>(audioData.samples.size());
    write_le<uint32_t>(out, file_size);
    out.write("WAVE", 4);

    // fmt chunk
    out.write("fmt ", 4);
    auto fmt_size = static_cast<uint32_t>(16);
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

    // data chunk
    out.write("data", 4);
    auto data_size = static_cast<uint32_t>(audioData.samples.size());
    write_le<uint32_t>(out, data_size);
    out.write(reinterpret_cast<const char*>(audioData.samples.data()), audioData.samples.size());
}

void FileWriters::writeIndexToFile(const boost::multiprecision::cpp_int& index, const std::string& outDir, const std::string& filename) {
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

    // Export bytes once (MSB-first) and produce all encodings from these bytes
    std::vector<uint8_t> bytes;
    boost::multiprecision::export_bits(index, std::back_inserter(bytes), 8, true);

    // Make a short stable stem from first bytes (hex)
    std::ostringstream stem_ss;
    stem_ss << std::hex << std::setfill('0');
    size_t take = std::min<size_t>(bytes.size(), 6);
    for (size_t i = 0; i < take; ++i) {
        stem_ss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    std::string stem = stem_ss.str();

    // choose base name: provided filename or generated stem
    std::string name = filename.empty() ? stem : filename;

    // write base64 textual representation as <dir>/<name>.b64.txt
    std::ofstream out((dir / (name + ".txt")).string());
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + (dir / (name + ".txt")).string());
    }

    // Use the centralized URL-safe base64 encoder from Utilities.h
    std::string base64Str = encodeBase64Url(bytes);
    out << base64Str << '\n';
    out.close();
}

} // namespace AudioBabel
