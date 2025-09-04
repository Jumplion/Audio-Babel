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

using boost::multiprecision::cpp_int;
namespace fs = std::filesystem;

namespace AudioBabel {

// Repository-wide numeric constants to avoid magic numbers in the implementation.
namespace {
    constexpr size_t   WAV_ID_LEN             = 4;             // 'RIFF'/'WAVE' id length
    constexpr size_t   FMT_CHUNK_MIN_SIZE     = 16;            // canonical PCM fmt chunk size
    constexpr size_t   HEADER_BYTES_CONST     = 4 + 2 + 2 + 8; // 16 bytes header layout
    constexpr size_t   BITS_PER_BYTE          = 8;
    constexpr int      BASE64_BITS            = 6;          // bits per base64 digit in our table
    constexpr uint32_t BYTE_MASK              = 0xFFu;      // mask for a single byte
    constexpr uint32_t BASE64_MASK            = 0x3Fu;      // mask for 6-bit base64 values
    constexpr uint16_t PCM_FORMAT_CODE        = 1;          // PCM format value
    constexpr uint16_t DEFAULT_NUM_CHANNELS   = 1;          // default assumed channels for sample vectors
    constexpr uint32_t CHUNK_SIZE_LIMIT       = (1u << 30); // sanity limit for chunk sizes
    constexpr uint32_t WAV_FILE_BASE_OVERHEAD = 36;         // base size used in RIFF size field
} // namespace

// ---------------------------------------------------------------------------
// 1) Binary IO helpers
// ---------------------------------------------------------------------------

template <typename T>
static void write_le(std::ostream& out, T value) {
    uint8_t buf[sizeof(T)];
    for (size_t index = 0; index < sizeof(T); ++index) {
        buf[index] = static_cast<uint8_t>((value >> (index * BITS_PER_BYTE)) & BYTE_MASK);
    }
    out.write(reinterpret_cast<const char*>(buf), sizeof(T));
}

static void write_u32_le(std::ostream& out, uint32_t value) {
    write_le<uint32_t>(out, value);
}
static void write_u16_le(std::ostream& out, uint16_t value) {
    write_le<uint16_t>(out, value);
}

template <typename T>
static T read_le(const char* ptr) {
    // use a 64-bit accumulator to avoid needing type_traits; safe for up to 64-bit reads
    uint64_t acc = 0;
    for (size_t index = 0; index < sizeof(T); ++index) {
        acc |= (uint64_t(static_cast<uint8_t>(ptr[index])) << (index * BITS_PER_BYTE));
    }
    return static_cast<T>(acc);
}

static uint16_t read_u16_le(const char* ptr) {
    return read_le<uint16_t>(ptr);
}
static uint32_t read_u32_le(const char* ptr) {
    return read_le<uint32_t>(ptr);
}

// ---------------------------------------------------------------------------
// 2) Construction / lifecycle
// ---------------------------------------------------------------------------

AudioIndex::AudioIndex() : audioData{} {}

AudioIndex::AudioIndex(const AudioIndex& other) : audioData(other.audioData) {}

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
    // build index integer and derive metadata
    try {
        cpp_int idx = AudioIndex::audioDataToIndex(index.audioData);
        index.metadata = AudioIndex::indexToMetadata(idx);
    } catch (...) {
        // non-fatal: leave metadata blank on error
    }
    return index;
}

AudioIndex::AudioData AudioIndex::extractAudioDataFromAudioFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open WAV file: " + path);
    }

    // Read RIFF header
    char riff[WAV_ID_LEN];
    in.read(riff, WAV_ID_LEN);
    if (std::strncmp(riff, "RIFF", WAV_ID_LEN) != 0) {
        throw std::runtime_error("Not a RIFF file");
    }

    // Read file size
    char tmp4[WAV_ID_LEN];
    in.read(tmp4, WAV_ID_LEN);

    // Read "WAVE" header
    char wave[WAV_ID_LEN];
    in.read(wave, WAV_ID_LEN);

    if (std::strncmp(wave, "WAVE", WAV_ID_LEN) != 0) {
        throw std::runtime_error("Not a WAVE file");
    }

    AudioData audioData{};

    /*
    This loop is a simple RIFF/WAV chunk parser that iterates over chunks until EOF or an error.
    Each iteration reads a 4‑byte chunk ID (into a non‑nul-terminated char[4]) and then reads the
    next 4 bytes into a temporary buffer sizeBuf. The code converts those four bytes to a host
    uint32_t via read_u32_le — this explicitly interprets the on‑disk size as little‑endian, which
    is why the size is first read into bytes and then converted rather than read directly into a
    uint32_t.

    After converting the chunk size the code performs a sanity check (rejecting sizes larger than
    1<<30) to avoid unreasonable allocations. It then dispatches on the chunk ID using std::strncmp
    with a length of 4, which is appropriate here because id is not nul‑terminated and the
    comparison only needs to match exactly four bytes.

    For the "fmt " chunk the code requires at least 16 bytes (the canonical minimum for PCM fmt) and
    reads the entire chunk into a temporary buffer. It then extracts fields using the provided
    little‑endian helpers: audio_format at offset 0, num_channels at offset 2, sample_rate at offset
    4, and bits_per_sample at offset 14. ote that these offsets correspond to the standard 16‑byte
    PCM fmt layout (AudioFormat, NumChannels, SampleRate, ByteRate, BlockAlign, BitsPerSample). If
    the fmt chunk is shorter than 16 bytes or the chunk is a non‑PCM/extended fmt, those fixed
    offsets can be invalid or insufficient — additional validation and handling for extensible fmt
    structures would be required.

    For the "data" chunk the code resizes audioData.samples to the chunk size and reads raw sample
    bytes into that buffer. Unknown chunks are skipped by advancing the stream by sz bytes plus one
    extra byte when sz is odd (sz + (sz & 1)), which correctly handles RIFF’s even‑byte padding
    rule. Throughout the loop the code uses placeholder comments for error handling; robust code
    should check every read/gcount and handle partial reads, malformed chunks, non‑PCM formats, and
    excessive sizes (to prevent OOM or denial‑of‑service).
    */
    // Read chunks
    while (in) {
        char id[WAV_ID_LEN];
        if (!in.read(id, WAV_ID_LEN)) {
            break;
        }

        // Read size as 4 bytes then interpret as little-endian to be portable
        char sizeBuf[WAV_ID_LEN];
        if (!in.read(sizeBuf, WAV_ID_LEN)) {
            break;
        }

        uint32_t chunkSize = read_u32_le(sizeBuf);

        // Guard against unreasonable sizes
        if (chunkSize > CHUNK_SIZE_LIMIT) {
            break;
        }

        if (std::strncmp(id, "fmt ", WAV_ID_LEN) == 0) {
            if (chunkSize < FMT_CHUNK_MIN_SIZE) {
                break;
            }
            std::vector<char> buf(chunkSize);
            if (!in.read(buf.data(), chunkSize)) {
                break;
            }
            audioData.audio_format = read_u16_le(buf.data());
            audioData.num_channels = read_u16_le(buf.data() + 2);
            audioData.sample_rate  = read_u32_le(buf.data() + 4);
            audioData.bit_rate     = read_u16_le(buf.data() + 14);
        } else if (std::strncmp(id, "data", WAV_ID_LEN) == 0) {
            audioData.samples.resize(chunkSize);
            if (!in.read(reinterpret_cast<char*>(audioData.samples.data()), chunkSize)) {
                break;
            }
        } else {
            // Skip unknown chunk and account for RIFF padding (chunks are even-sized)
            in.seekg(chunkSize + (chunkSize & 1), std::ios::cur);
            if (!in) {
                break;
            }
        }
    }

    if (audioData.samples.empty()) {
        throw std::runtime_error("No data chunk found in WAV");
    }

    // Compute number of frames
    size_t bytes_per_sample = audioData.bit_rate / BITS_PER_BYTE;
    audioData.num_frames    = audioData.samples.size() / (bytes_per_sample * audioData.num_channels);
    return audioData;
}

AudioIndex::AudioData AudioIndex::extractAudioDataFromSamples(const std::vector<int32_t>& samples, int sampleRate, int bitDepth) {
    AudioData audioData{};
    audioData.sample_rate  = static_cast<uint32_t>(sampleRate);
    audioData.bit_rate     = static_cast<uint16_t>(bitDepth);
    audioData.num_channels = DEFAULT_NUM_CHANNELS; // assuming mono input
    audioData.audio_format = PCM_FORMAT_CODE;      // PCM format

    // Convert int32 samples to bytes (little-endian)
    size_t bytes_per_sample = bitDepth / BITS_PER_BYTE;
    audioData.samples.resize(samples.size() * bytes_per_sample);
    for (size_t sampleIndex = 0; sampleIndex < samples.size(); ++sampleIndex) {
        int32_t sample = samples[sampleIndex];
        for (size_t byteIndex = 0; byteIndex < bytes_per_sample; ++byteIndex) {
            audioData.samples[(sampleIndex * bytes_per_sample) + byteIndex] =
                static_cast<uint8_t>((sample >> (byteIndex * BITS_PER_BYTE)) & BYTE_MASK);
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
    if (!(audioData.bit_rate == 8 || audioData.bit_rate == 16 || audioData.bit_rate == 32)) {
        throw std::runtime_error("Unsupported bit depth");
    }

    // Build pcm_int by concatenating samples. The canonical on-disk layout for
    // our serialized index is: [PCM_payload (MSB-first)] [16-byte big-endian header]
    // where the header occupies the least-significant bytes of the integer.
    // To construct the PCM portion efficiently we assemble a MSB-first byte
    // buffer and call boost::multiprecision::import_bits with the MSB flag set.
    size_t  bytes_per_sample = audioData.bit_rate / BITS_PER_BYTE;
    size_t  total_samples    = audioData.num_frames * audioData.num_channels;
    cpp_int pcm_int          = 0;

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
        boost::multiprecision::import_bits(pcm_int, pcm_be.begin(), pcm_be.end(), BITS_PER_BYTE, true);

        // populate debug info
        lastDebug.import_pcm_bytes      = pcm_be.size();
        lastDebug.import_expected_bytes = total_samples * bytes_per_sample;
    }

    auto t1_import               = std::chrono::steady_clock::now();
    lastDebug.audioDataToIndexMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1_import - t0_import).count());

    // Build explicit header bytes (big-endian): u32 sample_rate, u16 bit_depth, u16 num_channels,
    // u64 num_frames The header is placed into the least-significant HEADER_BYTES_CONST bytes so
    // consumers may extract it by masking the low bits. This explicit layout
    // avoids fragile bitwidth assumptions and makes deserialization robust.
    const size_t         HEADER_BYTES = HEADER_BYTES_CONST;
    std::vector<uint8_t> header_buf;
    header_buf.reserve(HEADER_BYTES);

    uint32_t sRate = audioData.sample_rate;
    header_buf.push_back(static_cast<uint8_t>((sRate >> (3 * BITS_PER_BYTE)) & BYTE_MASK));
    header_buf.push_back(static_cast<uint8_t>((sRate >> (2 * BITS_PER_BYTE)) & BYTE_MASK));
    header_buf.push_back(static_cast<uint8_t>((sRate >> (1 * BITS_PER_BYTE)) & BYTE_MASK));
    header_buf.push_back(static_cast<uint8_t>((sRate >> (0 * BITS_PER_BYTE)) & BYTE_MASK));

    uint16_t bitRate = audioData.bit_rate;
    header_buf.push_back(static_cast<uint8_t>((bitRate >> 8) & BYTE_MASK));
    header_buf.push_back(static_cast<uint8_t>((bitRate >> 8) & BYTE_MASK));
    header_buf.push_back(static_cast<uint8_t>((bitRate >> 0) & BYTE_MASK));

    uint16_t numChannels = audioData.num_channels;
    header_buf.push_back(static_cast<uint8_t>((numChannels >> 8) & BYTE_MASK));
    header_buf.push_back(static_cast<uint8_t>((numChannels >> 8) & BYTE_MASK));
    header_buf.push_back(static_cast<uint8_t>((numChannels >> 0) & BYTE_MASK));

    uint64_t numFrames = audioData.num_frames;
    for (int headerIndex = 7; headerIndex >= 0; --headerIndex) {
        header_buf.push_back(static_cast<uint8_t>((numFrames >> (headerIndex * BITS_PER_BYTE)) & BYTE_MASK));
    }

    // Convert header_buf to cpp_int (big-endian bytes)
    cpp_int header_int = 0;
    for (uint8_t headerByte : header_buf) {
        header_int <<= 8;
        header_int |= cpp_int(uint32_t(headerByte));
    }

    // Combine: shift pcm_int left by header_bits and OR header_int
    size_t  header_bits = HEADER_BYTES * BITS_PER_BYTE;
    cpp_int idx         = (pcm_int << header_bits) | header_int;
    return idx;
}

AudioIndex::AudioData AudioIndex::indexToAudioData(const boost::multiprecision::cpp_int& index) {
    // Header layout must match audioDataToIndex: HEADER_BYTES = 16
    const size_t HEADER_BYTES = HEADER_BYTES_CONST;
    const size_t HEADER_BITS  = HEADER_BYTES * BITS_PER_BYTE;

    // Extract header_int as the lower HEADER_BITS bits
    cpp_int mask       = (cpp_int(1) << HEADER_BITS) - 1;
    cpp_int header_int = index & mask;
    cpp_int pcm_int    = index >> HEADER_BITS;

    // Convert header_int to bytes (big-endian)
    std::vector<uint8_t> header_buf(HEADER_BYTES);
    cpp_int              tmp = header_int;
    for (int headerIndex = static_cast<int>(HEADER_BYTES) - 1; headerIndex >= 0; --headerIndex) {
        uint8_t byte            = static_cast<uint8_t>(static_cast<uint64_t>(tmp & BYTE_MASK));
        header_buf[headerIndex] = byte;
        tmp >>= BITS_PER_BYTE;
    }

    // Parse fields from header_buf (big-endian)
    uint32_t sample_rate =
        (uint32_t(header_buf[0]) << 24) | (uint32_t(header_buf[1]) << 16) | (uint32_t(header_buf[2]) << 8) | uint32_t(header_buf[3]);
    uint16_t bit_depth    = static_cast<uint16_t>((uint16_t(header_buf[4]) << 8) | uint16_t(header_buf[5]));
    uint16_t num_channels = static_cast<uint16_t>((uint16_t(header_buf[6]) << 8) | uint16_t(header_buf[7]));
    uint64_t num_frames   = 0;
    for (int i = 0; i < 8; ++i) {
        num_frames = (num_frames << BITS_PER_BYTE) | header_buf[8 + i];
    }

    if (!(bit_depth == 8 || bit_depth == 16 || bit_depth == 32)) {
        throw std::runtime_error("Unsupported bit depth in index");
    }

    // Compute number of samples
    size_t bytes_per_sample = bit_depth / BITS_PER_BYTE;
    size_t total_samples   = static_cast<size_t>(num_frames) * static_cast<size_t>(num_channels);
    cpp_int  sample_mask   = (cpp_int(1) << bit_depth) - 1;
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
                word = (word << BITS_PER_BYTE) | uint64_t(pcm_be_bytes[base + byteIndex]);
            }
            // Handle signed values depending on bit depth
            uint64_t signbit = uint64_t(1) << (bit_depth - 1);
            int64_t  sval    = 0;
            if (word & signbit) {
                sval = static_cast<int64_t>(word - (uint64_t(1) << bit_depth));
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
    audioData.audio_format  = 1;
    audioData.num_channels  = static_cast<uint16_t>(num_channels);
    audioData.sample_rate   = sample_rate;
    audioData.bit_rate      = static_cast<uint16_t>(bit_depth);
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

void AudioIndex::writeAudioDataToFile(const AudioData& audioData, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open output WAV: " + path);
    }

    // RIFF header
    out.write("RIFF", 4);
    uint32_t file_size = WAV_FILE_BASE_OVERHEAD + static_cast<uint32_t>(audioData.samples.size());
    write_u32_le(out, file_size);
    out.write("WAVE", 4);

    // fmt chunk
    out.write("fmt ", 4);
    uint32_t fmt_size = static_cast<uint32_t>(FMT_CHUNK_MIN_SIZE);
    write_u32_le(out, fmt_size);

    uint16_t audio_format = audioData.audio_format;
    write_u16_le(out, audio_format);
    write_u16_le(out, audioData.num_channels);
    write_u32_le(out, audioData.sample_rate);

    uint32_t byte_rate = audioData.sample_rate * audioData.num_channels * (audioData.bit_rate / BITS_PER_BYTE);
    write_u32_le(out, byte_rate);

    uint16_t block_align = static_cast<uint16_t>(audioData.num_channels * (audioData.bit_rate / BITS_PER_BYTE));
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
    return audioData.audio_format == other.audioData.audio_format && audioData.num_channels == other.audioData.num_channels &&
           audioData.sample_rate == other.audioData.sample_rate && audioData.bit_rate == other.audioData.bit_rate &&
           audioData.num_frames == other.audioData.num_frames && audioData.samples == other.audioData.samples;
}

bool AudioIndex::operator!=(const AudioIndex& other) const {
    return !(*this == other);
}

// ---------------------------------------------------------------------------
// 9) Index representation helpers
// ---------------------------------------------------------------------------

void AudioIndex::writeIndexRepresentations(const boost::multiprecision::cpp_int& index, const std::string& outDir, const std::string& filename) {
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
    for (size_t i = 0; i < take; ++i) stem_ss << std::setw(2) << static_cast<int>(bytes[i]);
    std::string stem = stem_ss.str();

    // choose base name: provided filename or generated stem
    std::string name = filename.empty() ? stem : filename;

    // write base64 textual representation as <dir>/<name>.b64.txt
    std::ofstream out((dir / (name + ".txt")).string());
    if (!out) return;

    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    uint32_t          acc   = 0;
    int               acc_bits = 0;
    for (uint8_t byte : bytes) {
        acc = (acc << BITS_PER_BYTE) | byte;
        acc_bits += static_cast<int>(BITS_PER_BYTE);
        while (acc_bits >= BASE64_BITS) {
            acc_bits -= BASE64_BITS;
            uint8_t idx = static_cast<uint8_t>((acc >> acc_bits) & 0x3F);
            out.put(b64[idx]);
        }
    }
    if (acc_bits > 0) {
        uint8_t idx = static_cast<uint8_t>((acc << (BASE64_BITS - acc_bits)) & 0x3F);
        out.put(b64[idx]);
    }
    out.put('\n');
    out.close();
}

// ---------------------------------------------------------------------------
// 10) Metadata derivation
// ---------------------------------------------------------------------------

AudioIndex::Metadata AudioIndex::indexToMetadata(const boost::multiprecision::cpp_int& index) {
    std::vector<uint8_t> bytes;
    boost::multiprecision::export_bits(index, std::back_inserter(bytes), 8, true);

    auto mk = [&](size_t off, size_t len) {
        std::string s;
        for (size_t i = 0; i < len; ++i) {
            uint8_t b = (off + i < bytes.size()) ? bytes[off + i] : 0;
            char c = static_cast<char>((b % 36) < 10 ? ('0' + (b % 10)) : ('a' + ((b % 36) - 10)));
            s.push_back(c);
        }
        return s;
    };

    Metadata m;
    if (bytes.empty()) {
        m.genre = "g0";
        m.artist = "a0";
        m.album = "al0";
        m.track = "t0";
        return m;
    }

    m.genre = mk(0, 6);
    m.artist = mk(6, 8);
    m.album = mk(14, 8);
    m.track = mk(22, 6);

    // generate a tiny SVG cover from first bytes
    std::string svg = "<svg xmlns='http://www.w3.org/2000/svg' width='256' height='256'>";
    svg += "<rect width='100%' height='100%' fill='#";
    unsigned int color = 0;
    for (size_t i = 0; i < 3; ++i) color = (color << 8) | (i < bytes.size() ? bytes[i] : 0);
    const char* hex = "0123456789abcdef";
    for (int i = 5; i >= 0; --i) {
        unsigned int nib = (color >> (i * 4)) & 0xF;
        svg.push_back(hex[nib]);
    }
    svg += "'/><text x='50%' y='50%' font-size='20' text-anchor='middle' fill='#fff' dominant-baseline='middle'>";
    svg += m.track;
    svg += "</text></svg>";
    m.cover.assign(svg.begin(), svg.end());
    return m;
}

} // namespace AudioBabel
