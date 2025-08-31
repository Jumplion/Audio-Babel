#include "AudioIndex.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <random>
#include <cstring>
#include <algorithm>
#include <chrono>

#include <boost/multiprecision/cpp_int.hpp>
#include <vector>
#include <cstdint>
#include <stdexcept>

using boost::multiprecision::cpp_int;
namespace fs = std::filesystem;

namespace AudioBabel {

// ---------------------------------------------------------------------------
// 1) Binary IO helpers
// ---------------------------------------------------------------------------

template<typename T>
static void write_le(std::ostream& out, T value) {
    uint8_t buf[sizeof(T)];
    for (size_t index = 0; index < sizeof(T); ++index) {
        buf[index] = static_cast<uint8_t>((value >> (index * 8)) & 0xFF);
    }
    out.write(reinterpret_cast<const char*>(buf), sizeof(T));
}

static void write_u32_le(std::ostream& out, uint32_t value) { write_le<uint32_t>(out, value); }
static void write_u16_le(std::ostream& out, uint16_t value) { write_le<uint16_t>(out, value); }

template<typename T>
static T read_le(const char* ptr) {
    // use a 64-bit accumulator to avoid needing type_traits; safe for up to 64-bit reads
    uint64_t acc = 0;
    for (size_t index = 0; index < sizeof(T); ++index) {
        acc |= (uint64_t(static_cast<uint8_t>(ptr[index])) << (index * 8));
    }
    return static_cast<T>(acc);
}

static uint16_t read_u16_le(const char* ptr) { return read_le<uint16_t>(ptr); }
static uint32_t read_u32_le(const char* ptr) { return read_le<uint32_t>(ptr); }

// ---------------------------------------------------------------------------
// 2) Construction / lifecycle
// ---------------------------------------------------------------------------

AudioIndex::AudioIndex() : audioData{} { }

AudioIndex::AudioIndex(const AudioIndex& other) : audioData(other.audioData) { }

AudioIndex& AudioIndex::operator=(const AudioIndex& other) {
    if (this != &other) {
    audioData = other.audioData;
    }
    return *this;
}

AudioIndex::~AudioIndex() {
    audioData.samples.clear();
}

// ---------------------------------------------------------------------------
// 3) Factory functions
// ---------------------------------------------------------------------------

AudioIndex AudioIndex::fromAudioSamples(const std::vector<int32_t>& samples, int sampleRate, int bitDepth) {
    AudioIndex index;
    index.audioData = AudioIndex::extractAudioDataFromSamples(samples, sampleRate, bitDepth);
    return index;
}

AudioIndex::AudioData AudioIndex::extractAudioDataFromAudioFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open WAV file: " + path);
    }

    // Read RIFF header
    char riff[4]; in.read(riff, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0) {
        throw std::runtime_error("Not a RIFF file");
    }

    // Read file size
    char tmp4[4]; in.read(tmp4, 4);

    // Read "WAVE" header
    char wave[4]; in.read(wave, 4);
    
    if (std::strncmp(wave, "WAVE", 4) != 0) {
        throw std::runtime_error("Not a WAVE file");
    }

    AudioData audioData{};

/*
This loop is a simple RIFF/WAV chunk parser that iterates over chunks until EOF or an error. 
Each iteration reads a 4‑byte chunk ID (into a non‑nul-terminated char[4]) and then reads the 
next 4 bytes into a temporary buffer sizeBuf. The code converts those four bytes to a host 
uint32_t via read_u32_le — this explicitly interprets the on‑disk size as little‑endian, which 
is why the size is first read into bytes and then converted rather than read directly into a uint32_t.

After converting the chunk size the code performs a sanity check (rejecting sizes larger than 1<<30) 
to avoid unreasonable allocations. It then dispatches on the chunk ID using std::strncmp with a length of 4, 
which is appropriate here because id is not nul‑terminated and the comparison only needs to match exactly four bytes.

For the "fmt " chunk the code requires at least 16 bytes (the canonical minimum for PCM fmt) and reads 
the entire chunk into a temporary buffer. It then extracts fields using the provided little‑endian helpers: 
audio_format at offset 0, num_channels at offset 2, sample_rate at offset 4, and bits_per_sample at offset 14. 
ote that these offsets correspond to the standard 16‑byte PCM fmt layout (AudioFormat, NumChannels, SampleRate, 
ByteRate, BlockAlign, BitsPerSample). If the fmt chunk is shorter than 16 bytes or the chunk is a non‑PCM/extended fmt, 
those fixed offsets can be invalid or insufficient — additional validation and handling for extensible fmt structures would be required.

For the "data" chunk the code resizes audioData.samples to the chunk size and reads raw sample bytes into that buffer. 
Unknown chunks are skipped by advancing the stream by sz bytes plus one extra byte when sz is odd (sz + (sz & 1)), 
which correctly handles RIFF’s even‑byte padding rule. Throughout the loop the code uses placeholder comments for error handling; 
robust code should check every read/gcount and handle partial reads, malformed chunks, non‑PCM formats, and excessive sizes 
(to prevent OOM or denial‑of‑service).
*/
    // Read chunks
    while (in) {
        char id[4];
        if (!in.read(id, 4)) {
            break;
        }

        // Read size as 4 bytes then interpret as little-endian to be portable
        char sizeBuf[4];
        if (!in.read(sizeBuf, 4)) {
            break;
        }
        uint32_t sz = read_u32_le(sizeBuf);

        // Guard against unreasonable sizes
        if (sz > (1u << 30)) {
            /* handle error */
            break;
        }

        if (std::strncmp(id, "fmt ", 4) == 0) {
            if (sz < 16) { /* handle malformed fmt chunk */
                break;
            }
            std::vector<char> buf(sz);
            if (!in.read(buf.data(), sz)) { /* handle partial read */
                break;
            }
            audioData.audio_format = read_u16_le(buf.data());
            audioData.num_channels = read_u16_le(buf.data()+2);
            audioData.sample_rate = read_u32_le(buf.data()+4);
            audioData.bit_rate = read_u16_le(buf.data()+14);
        } else if (std::strncmp(id, "data", 4) == 0) {
            audioData.samples.resize(sz);
            if (!in.read(reinterpret_cast<char*>(audioData.samples.data()), sz)) { /* handle partial read */
                break;
            }
        } else {
            // Skip unknown chunk and account for RIFF padding (chunks are even-sized)
            in.seekg(sz + (sz & 1), std::ios::cur);
            if (!in) {
                break;
            }
        }
    }

    if (audioData.samples.empty()) {
        throw std::runtime_error("No data chunk found in WAV");
    }

    // Compute number of frames
    size_t bytes_per_sample = audioData.bit_rate / 8;
    audioData.num_frames = audioData.samples.size() / (bytes_per_sample * audioData.num_channels);
    return audioData;
}

AudioIndex::AudioData AudioIndex::extractAudioDataFromSamples(const std::vector<int32_t>& samples, int sampleRate, int bitDepth) {
    AudioData audioData{};
    audioData.sample_rate = static_cast<uint32_t>(sampleRate);
    audioData.bit_rate = static_cast<uint16_t>(bitDepth);
    audioData.num_channels = 1; // assuming mono input
    audioData.audio_format = 1; // PCM format

    // Convert int32 samples to bytes (little-endian)
    size_t bytes_per_sample = bitDepth / 8;
    audioData.samples.resize(samples.size() * bytes_per_sample);
    for (size_t sampleIndex = 0; sampleIndex < samples.size(); ++sampleIndex) {
        int32_t sample = samples[sampleIndex];
        for (size_t byteIndex = 0; byteIndex < bytes_per_sample; ++byteIndex) {
            audioData.samples[(sampleIndex * bytes_per_sample) + byteIndex] = static_cast<uint8_t>((sample >> (byteIndex * 8)) & 0xFF);
        }
    }

    // compute number of frames
    audioData.num_frames = samples.size() / audioData.num_channels;
    return audioData;
}

// Debug storage for import/export statistics
static AudioIndex::DebugInfo lastDebug;

AudioIndex::DebugInfo AudioIndex::getLastDebugInfo() {
    return lastDebug;
}

void AudioIndex::clearLastDebugInfo() {
    lastDebug = DebugInfo();
}

boost::multiprecision::cpp_int AudioIndex::audioDataToIndex(const AudioIndex::AudioData& audioData) {

    // only support 8,16,32
    if (!(audioData.bit_rate==8 || audioData.bit_rate==16 || audioData.bit_rate==32)) {
        throw std::runtime_error("Unsupported bit depth");
    }

    // Build pcm_int by concatenating samples. The canonical on-disk layout for
    // our serialized index is: [PCM_payload (MSB-first)] [16-byte big-endian header]
    // where the header occupies the least-significant bytes of the integer.
    // To construct the PCM portion efficiently we assemble a MSB-first byte
    // buffer and call boost::multiprecision::import_bits with the MSB flag set.
    size_t bytes_per_sample = audioData.bit_rate/8;
    size_t total_samples = audioData.num_frames * audioData.num_channels;
    cpp_int pcm_int = 0;

    auto t0_import = std::chrono::steady_clock::now();
    if (total_samples > 0) {
        std::vector<uint8_t> pcm_be;
        pcm_be.reserve(total_samples * bytes_per_sample);
        
        // audioData.samples stores little-endian bytes per sample; we need
        // to append each sample's bytes in big-endian order so the first
        // sample ends up as the most-significant bytes in the resulting integer.
        for (size_t sampleIndex = 0; sampleIndex < total_samples; ++sampleIndex) {
            size_t offset = sampleIndex * bytes_per_sample;
            for (int byteIndex = static_cast<int>(bytes_per_sample) - 1; byteIndex >= 0; --byteIndex) {
                pcm_be.push_back(audioData.samples[offset + byteIndex]);
            }
        }
        
        // Import bytes into cpp_int (MSB-first)
        boost::multiprecision::import_bits(pcm_int, pcm_be.begin(), pcm_be.end(), 8, true);

        // populate debug info
        lastDebug.import_pcm_bytes = pcm_be.size();
        lastDebug.import_expected_bytes = total_samples * bytes_per_sample;
    }

    auto t1_import = std::chrono::steady_clock::now();
    lastDebug.audioDataToIndexMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1_import - t0_import).count());

    // Build explicit header bytes (big-endian): u32 sample_rate, u16 bit_depth, u16 num_channels, u64 num_frames
    // The header is placed into the least-significant 128 bits (16 bytes) so
    // consumers may extract it by masking the low bits. This explicit layout
    // avoids fragile bitwidth assumptions and makes deserialization robust.
    const size_t HEADER_BYTES = 4 + 2 + 2 + 8; // 16 bytes
    std::vector<uint8_t> header_buf;
    header_buf.reserve(HEADER_BYTES);

    uint32_t sr = audioData.sample_rate;
    header_buf.push_back(static_cast<uint8_t>((sr >> 24) & 0xFF));
    header_buf.push_back(static_cast<uint8_t>((sr >> 16) & 0xFF));
    header_buf.push_back(static_cast<uint8_t>((sr >> 8) & 0xFF));
    header_buf.push_back(static_cast<uint8_t>((sr >> 0) & 0xFF));

    uint16_t bd = audioData.bit_rate;
    header_buf.push_back(static_cast<uint8_t>((bd >> 8) & 0xFF));
    header_buf.push_back(static_cast<uint8_t>((bd >> 0) & 0xFF));

    uint16_t nc = audioData.num_channels;
    header_buf.push_back(static_cast<uint8_t>((nc >> 8) & 0xFF));
    header_buf.push_back(static_cast<uint8_t>((nc >> 0) & 0xFF));

    uint64_t nf = audioData.num_frames;
    for (int headerIndex = 7; headerIndex >= 0; --headerIndex) {
        header_buf.push_back(static_cast<uint8_t>((nf >> (headerIndex*8)) & 0xFF));
    }

    // Convert header_buf to cpp_int (big-endian bytes)
    cpp_int header_int = 0;
    for (uint8_t headerByte : header_buf) {
        header_int <<= 8;
        header_int |= cpp_int(uint32_t(headerByte));
    }

    // Combine: shift pcm_int left by header_bits and OR header_int
    size_t header_bits = HEADER_BYTES * 8;
    cpp_int idx = (pcm_int << header_bits) | header_int;
    return idx;
}

AudioIndex::AudioData AudioIndex::indexToAudioData(const boost::multiprecision::cpp_int& index) {
    // Header layout must match audioDataToIndex: HEADER_BYTES = 16
    const size_t HEADER_BYTES = 4 + 2 + 2 + 8;
    const size_t HEADER_BITS = HEADER_BYTES * 8;

    // Extract header_int as the lower HEADER_BITS bits
    cpp_int mask = (cpp_int(1) << HEADER_BITS) - 1;
    cpp_int header_int = index & mask;
    cpp_int pcm_int = index >> HEADER_BITS;

    // Convert header_int to bytes (big-endian)
    std::vector<uint8_t> header_buf(HEADER_BYTES);
    cpp_int tmp = header_int;
    for (int headerIndex = static_cast<int>(HEADER_BYTES) - 1; headerIndex >= 0; --headerIndex) {
        uint8_t byte = static_cast<uint8_t>(static_cast<uint64_t>(tmp & 0xFF));
        header_buf[headerIndex] = byte;
        tmp >>= 8;
    }

    // Parse fields from header_buf (big-endian)
    uint32_t sample_rate = (uint32_t(header_buf[0]) << 24) | (uint32_t(header_buf[1]) << 16) | (uint32_t(header_buf[2]) << 8) | uint32_t(header_buf[3]);
    uint16_t bit_depth = static_cast<uint16_t>((uint16_t(header_buf[4]) << 8) | uint16_t(header_buf[5]));
    uint16_t num_channels = static_cast<uint16_t>((uint16_t(header_buf[6]) << 8) | uint16_t(header_buf[7]));
    uint64_t num_frames = 0;
    for (int i = 0; i < 8; ++i) {
        num_frames = (num_frames << 8) | header_buf[8 + i];
    }

    if (!(bit_depth==8 || bit_depth==16 || bit_depth==32)) {
        throw std::runtime_error("Unsupported bit depth in index");
    }

    // Compute number of samples
    size_t total_samples = static_cast<size_t>(num_frames) * static_cast<size_t>(num_channels);
    cpp_int sample_mask = (cpp_int(1) << bit_depth) - 1;
    std::vector<uint64_t> samples;
    samples.reserve(total_samples);

    // Extract samples: use export_bits to extract PCM bytes in big-endian
    if (total_samples > 0) {
        // export_bits writes least-significant byte first by default; request MSB-first
        std::vector<uint8_t> pcm_be_bytes;
        pcm_be_bytes.reserve(total_samples * (bit_depth/8));
        boost::multiprecision::export_bits(pcm_int, std::back_inserter(pcm_be_bytes), 8, true);

        // pcm_be_bytes now contains samples in big-endian sample order
        // We need to split into samples and convert each to host-endian little-endian byte order
        size_t bytes_per_sample = bit_depth / 8;
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

        // iterate samples in order and convert to unsigned sample words
            for (size_t sampleIndex = 0; sampleIndex < total_samples; ++sampleIndex) {
                size_t base = sampleIndex * bytes_per_sample;
                uint64_t word = 0;
                for (size_t byteIndex = 0; byteIndex < bytes_per_sample; ++byteIndex) {
                    word = (word << 8) | uint64_t(pcm_be_bytes[base + byteIndex]);
                }
                // Handle signed values depending on bit depth
                uint64_t signbit = uint64_t(1) << (bit_depth - 1);
                int64_t sval = 0;
                if (word & signbit) {
                    sval = static_cast<int64_t>(word - (uint64_t(1) << bit_depth));
                } else {
                    sval = static_cast<int64_t>(word);
                }
                samples.push_back(static_cast<uint64_t>(sval & ((1ULL << bit_depth) - 1)));
            }
        std::cout << std::endl;

        // record export stats
        lastDebug.export_pcm_bytes = pcm_be_bytes.size();
        lastDebug.export_expected_bytes = expected_bytes;
    }

    // pack bytes
    AudioData audioData{};
    audioData.audio_format = 1;
    audioData.num_channels = static_cast<uint16_t>(num_channels);
    audioData.sample_rate = sample_rate;
    audioData.bit_rate = static_cast<uint16_t>(bit_depth);
    size_t bytes_per_sample = bit_depth/8;
    audioData.samples.resize(total_samples * bytes_per_sample);

    for (size_t sampleIndex=0; sampleIndex<total_samples; sampleIndex++){
        uint64_t v = samples[sampleIndex];
        for (size_t byteIndex=0; byteIndex<bytes_per_sample; byteIndex++){
            audioData.samples[(sampleIndex*bytes_per_sample) + byteIndex] = static_cast<uint8_t>((v >> (8*byteIndex)) & 0xFF);
        }
    }
    audioData.num_frames = static_cast<size_t>(num_frames);
    return audioData;
}

void AudioIndex::writeAudioDataToFile(const AudioData& audioData, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open output WAV: " + path);
    }
    
    // RIFF header
    out.write("RIFF", 4);
    uint32_t file_size = 36 + (uint32_t)audioData.samples.size();
    write_u32_le(out, file_size);
    out.write("WAVE", 4);
    
    // fmt chunk
    out.write("fmt ", 4);
    uint32_t fmt_size = 16;
    write_u32_le(out, fmt_size);

    uint16_t audio_format = audioData.audio_format;
    write_u16_le(out, audio_format);
    write_u16_le(out, audioData.num_channels);
    write_u32_le(out, audioData.sample_rate);

    uint32_t byte_rate = audioData.sample_rate * audioData.num_channels * (audioData.bit_rate/8);
    write_u32_le(out, byte_rate);

    uint16_t block_align = static_cast<uint16_t>(audioData.num_channels * (audioData.bit_rate/8));
    write_u16_le(out, block_align);
    write_u16_le(out, audioData.bit_rate);

    // data chunk
    out.write("data", 4);
    uint32_t data_size = static_cast<uint32_t>(audioData.samples.size());
    write_u32_le(out, data_size);
    out.write(reinterpret_cast<const char*>(audioData.samples.data()), audioData.samples.size());
}

// ---------------------------------------------------------------------------
// 8) Comparison operators
// ---------------------------------------------------------------------------

bool AudioIndex::operator==(const AudioIndex& other) const {
    // Compare audioData fields (audioData is the single source of truth)
    if (audioData.audio_format != other.audioData.audio_format) {
        return false;
    }
    if (audioData.num_channels != other.audioData.num_channels) {
        return false;
    }
    if (audioData.sample_rate != other.audioData.sample_rate) {
        return false;
    }
    if (audioData.bit_rate != other.audioData.bit_rate) {
        return false;
    }
    if (audioData.num_frames != other.audioData.num_frames) {
        return false;
    }

    // Compare sample payload sizes first to short-circuit heavy comparisons
    if (audioData.samples.size() != other.audioData.samples.size()) {
        return false;
    }
    return audioData.samples == other.audioData.samples;
}

bool AudioIndex::operator!=(const AudioIndex& other) const {
    return !(*this == other);
}

// ---------------------------------------------------------------------------
// 9) Index representation helpers
// ---------------------------------------------------------------------------

void AudioIndex::writeIndexRepresentations(const boost::multiprecision::cpp_int& index, const std::string& outPrefix) {
    // Ensure target directory exists: write under cpp/tests/indexes/<basename>
    fs::path baseDir = fs::path("cpp") / "tests" / "indexes";
    try { fs::create_directories(baseDir); } catch(...) {}
    fs::path stem = fs::path(outPrefix).filename();
    fs::path targetPrefix = baseDir / stem;

    std::ofstream out(targetPrefix.string() + ".txt");
    if (!out) {
        return;
    }

    // Export bytes once (MSB-first) and produce all encodings from these bytes
    std::vector<uint8_t> bytes;
    boost::multiprecision::export_bits(index, std::back_inserter(bytes), 8, true);

    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    size_t i = 0;
    uint32_t acc = 0; int acc_bits = 0;
    for (uint8_t byte : bytes) {
        acc = (acc << 8) | byte;
        acc_bits += 8;
        while (acc_bits >= 6) {
            acc_bits -= 6;
            uint8_t idx = static_cast<uint8_t>((acc >> acc_bits) & 0x3F);
            out.put(b64[idx]);
        }
    }
    if (acc_bits > 0) {
        uint8_t idx = static_cast<uint8_t>((acc << (6 - acc_bits)) & 0x3F);
        out.put(b64[idx]);
    }
    out.put('\n');
    out.close();
}

} // namespace AudioBabel
