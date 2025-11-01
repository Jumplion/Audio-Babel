/**
 * @file test_wav_parsing_new.cpp
 * @brief Unit tests for WAV file parsing, header validation, and edge cases.
 *
 * Tests WAV file parsing functionality including:
 *  - WAV header correctness on export
 *  - fmt chunk variants (extra/odd bytes)
 *  - odd-sized unknown chunk with padding
 *  - truncated/malformed files
 *  - unsupported bit depths
 *  - declared data chunk larger than actual
 * 
 * Migrated to Catch2 v3 framework.
 */

#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace AudioBabel;

// ============================================================================
// Helper functions to reduce code duplication in WAV file generation
// ============================================================================

namespace {

/// Write a 32-bit little-endian integer to an output stream
inline void write_u32_le(std::ofstream& out, uint32_t value) {
    out.put(static_cast<char>(value & 0xFF));
    out.put(static_cast<char>((value >> 8) & 0xFF));
    out.put(static_cast<char>((value >> 16) & 0xFF));
    out.put(static_cast<char>((value >> 24) & 0xFF));
}

/// Write a 16-bit little-endian integer to an output stream
inline void write_u16_le(std::ofstream& out, uint16_t value) {
    out.put(static_cast<char>(value & 0xFF));
    out.put(static_cast<char>((value >> 8) & 0xFF));
}

/// Write RIFF header to output stream
inline void write_riff_header(std::ofstream& out, uint32_t riff_size) {
    out.write("RIFF", 4);
    write_u32_le(out, riff_size);
    out.write("WAVE", 4);
}

/// Write fmt chunk to output stream
inline void write_fmt_chunk(
    std::ofstream& out, uint32_t fmt_size, uint16_t audio_format, uint16_t num_channels, uint32_t sample_rate, uint16_t bits_per_sample) {
    uint32_t byte_rate   = sample_rate * num_channels * (bits_per_sample / 8);
    uint16_t block_align = num_channels * (bits_per_sample / 8);

    out.write("fmt ", 4);
    write_u32_le(out, fmt_size);
    write_u16_le(out, audio_format);
    write_u16_le(out, num_channels);
    write_u32_le(out, sample_rate);
    write_u32_le(out, byte_rate);
    write_u16_le(out, block_align);
    write_u16_le(out, bits_per_sample);
}

/// Write data chunk header and data to output stream
inline void write_data_chunk(std::ofstream& out, const std::vector<uint8_t>& data) {
    out.write("data", 4);
    write_u32_le(out, static_cast<uint32_t>(data.size()));
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
}

/// Write data chunk with custom declared size (for testing mismatches)
inline void write_data_chunk_with_size(std::ofstream& out, const std::vector<uint8_t>& data, uint32_t declared_size) {
    out.write("data", 4);
    write_u32_le(out, declared_size);
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
}

/// Write JUNK chunk to output stream
inline void write_junk_chunk(std::ofstream& out, uint32_t junk_size, bool add_padding = false) {
    out.write("JUNK", 4);
    write_u32_le(out, junk_size);
    // Write arbitrary junk data
    for (uint32_t i = 0; i < junk_size; i++) {
        out.put(static_cast<char>(0xAA + (i % 16)));
    }
    // Add padding byte if chunk size is odd
    if (add_padding && (junk_size % 2 == 1)) {
        out.put(static_cast<char>(0x00));
    }
}

} // anonymous namespace

// ============================================================================
// Test Cases
// ============================================================================

TEST_CASE("WAV export: header correctness", "[wav][export][header]") {
    // Create test audio data
    AudioIndex::AudioData audioData{};
    audioData.sample_rate  = 22050;
    audioData.bit_rate     = 16;
    audioData.num_channels = 1;
    audioData.audio_format = 1;
    audioData.num_frames   = 3;
    size_t data_bytes      = audioData.num_frames * audioData.num_channels * (audioData.bit_rate / 8);
    audioData.samples.resize(data_bytes);
    for (size_t i = 0; i < data_bytes; ++i) {
        audioData.samples[i] = static_cast<uint8_t>(i + 1);
    }

    TempFile tmp(make_temp_path("temp_export_header_test.wav"));
    AudioIndex::exportAudioDataToWav(audioData, tmp.path());

    // Read the header back
    std::ifstream in(tmp.path(), std::ios::binary);
    REQUIRE(in);

    std::vector<uint8_t> hdr(44);
    in.read(reinterpret_cast<char*>(hdr.data()), static_cast<std::streamsize>(hdr.size()));
    REQUIRE(in);

    SECTION("RIFF header") {
        REQUIRE(hdr[0] == 'R');
        REQUIRE(hdr[1] == 'I');
        REQUIRE(hdr[2] == 'F');
        REQUIRE(hdr[3] == 'F');
    }

    SECTION("File size") {
        uint32_t file_size = static_cast<uint32_t>(hdr[4]) | (static_cast<uint32_t>(hdr[5]) << 8) | (static_cast<uint32_t>(hdr[6]) << 16) |
                             (static_cast<uint32_t>(hdr[7]) << 24);
        uint32_t expected_file_size = 36U + static_cast<uint32_t>(audioData.samples.size());
        REQUIRE(file_size == expected_file_size);
    }

    SECTION("WAVE format tag") {
        REQUIRE(hdr[8] == 'W');
        REQUIRE(hdr[9] == 'A');
        REQUIRE(hdr[10] == 'V');
        REQUIRE(hdr[11] == 'E');
    }

    SECTION("fmt chunk id") {
        REQUIRE(hdr[12] == 'f');
        REQUIRE(hdr[13] == 'm');
        REQUIRE(hdr[14] == 't');
        REQUIRE(hdr[15] == ' ');
    }

    SECTION("fmt chunk size") {
        uint32_t fmt_size = static_cast<uint32_t>(hdr[16]) | (static_cast<uint32_t>(hdr[17]) << 8) | (static_cast<uint32_t>(hdr[18]) << 16) |
                            (static_cast<uint32_t>(hdr[19]) << 24);
        REQUIRE(fmt_size == 16U);
    }

    SECTION("Audio format (PCM)") {
        uint16_t audio_format = static_cast<uint16_t>(hdr[20]) | (static_cast<uint16_t>(hdr[21]) << 8);
        REQUIRE(audio_format == audioData.audio_format);
    }

    SECTION("Number of channels") {
        uint16_t num_channels = static_cast<uint16_t>(hdr[22]) | (static_cast<uint16_t>(hdr[23]) << 8);
        REQUIRE(num_channels == audioData.num_channels);
    }

    SECTION("Sample rate") {
        uint32_t sample_rate = static_cast<uint32_t>(hdr[24]) | (static_cast<uint32_t>(hdr[25]) << 8) | (static_cast<uint32_t>(hdr[26]) << 16) |
                               (static_cast<uint32_t>(hdr[27]) << 24);
        REQUIRE(sample_rate == audioData.sample_rate);
    }

    SECTION("Bits per sample") {
        uint16_t bits_per_sample = static_cast<uint16_t>(hdr[34]) | (static_cast<uint16_t>(hdr[35]) << 8);
        REQUIRE(bits_per_sample == audioData.bit_rate);
    }

    SECTION("data chunk id") {
        REQUIRE(hdr[36] == 'd');
        REQUIRE(hdr[37] == 'a');
        REQUIRE(hdr[38] == 't');
        REQUIRE(hdr[39] == 'a');
    }

    SECTION("data chunk size") {
        uint32_t data_size = static_cast<uint32_t>(hdr[40]) | (static_cast<uint32_t>(hdr[41]) << 8) | (static_cast<uint32_t>(hdr[42]) << 16) |
                             (static_cast<uint32_t>(hdr[43]) << 24);
        REQUIRE(data_size == static_cast<uint32_t>(audioData.samples.size()));
    }
}

TEST_CASE("WAV parsing: fmt chunk with extra bytes", "[wav][parsing][fmt][tolerance]") {
    TempFile tmp(make_temp_path("temp_fmt_extra.wav"));

    uint16_t             audio_format    = 1;
    uint16_t             num_channels    = 1;
    uint32_t             sample_rate     = 44100;
    uint16_t             bits_per_sample = 16;
    std::vector<uint8_t> data            = {0x11, 0x22, 0x33, 0x44};

    uint32_t fmt_size  = 18; // 2 extra bytes beyond canonical 16
    uint32_t riff_size = 4 + (8 + fmt_size) + (8 + static_cast<uint32_t>(data.size()));

    std::ofstream out(tmp.path(), std::ios::binary);
    REQUIRE(out);

    write_riff_header(out, riff_size);
    write_fmt_chunk(out, fmt_size, audio_format, num_channels, sample_rate, bits_per_sample);

    // extra two bytes (should be tolerated by parser)
    out.put(static_cast<char>(0x55));
    out.put(static_cast<char>(0x66));

    write_data_chunk(out, data);
    out.close();

    // Parser should tolerate extra bytes and successfully extract audio data
    auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());

    SECTION("Sample rate matches") {
        REQUIRE(ad.sample_rate == sample_rate);
    }

    SECTION("Bit depth matches") {
        REQUIRE(ad.bit_rate == bits_per_sample);
    }

    SECTION("Number of channels matches") {
        REQUIRE(ad.num_channels == num_channels);
    }

    SECTION("Data size matches") {
        REQUIRE(ad.samples.size() == data.size());
    }
}

TEST_CASE("WAV parsing: fmt chunk variants", "[wav][parsing][fmt][tolerance]") {
    SECTION("Odd-sized fmt chunk (17 bytes) with one extra byte") {
        TempFile      tmp(make_temp_path("temp_fmt_odd17.wav"));
        std::ofstream out(tmp.path(), std::ios::binary);
        REQUIRE(out);

        uint16_t             num_channels    = 1;
        uint32_t             sample_rate     = 44100;
        uint16_t             bits_per_sample = 16;
        std::vector<uint8_t> data            = {0x11, 0x22};
        uint32_t             fmt_size        = 17; // odd

        uint32_t riff_size = 4 + (8 + fmt_size) + (8 + static_cast<uint32_t>(data.size()));
        write_riff_header(out, riff_size);
        write_fmt_chunk(out, fmt_size, 1, num_channels, sample_rate, bits_per_sample);
        out.put(static_cast<char>(0x7F)); // one extra byte
        write_data_chunk(out, data);
        out.close();

        auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
        REQUIRE(ad.sample_rate == sample_rate);
        REQUIRE(ad.bit_rate == bits_per_sample);
        REQUIRE(ad.num_channels == num_channels);
        REQUIRE(ad.samples.size() == data.size());
    }

    SECTION("Invalid byte_rate (zero) should not crash parser") {
        TempFile      tmp(make_temp_path("temp_fmt_byte_rate0.wav"));
        std::ofstream out(tmp.path(), std::ios::binary);
        REQUIRE(out);

        uint16_t             num_channels    = 1;
        uint32_t             sample_rate     = 22050;
        uint16_t             bits_per_sample = 16;
        std::vector<uint8_t> data            = {0x55, 0x66, 0x77};

        uint32_t riff_size = 4 + (8 + 16) + (8 + static_cast<uint32_t>(data.size()));
        write_riff_header(out, riff_size);

        // Write fmt chunk manually with invalid byte_rate
        write_u32_le(out, 0x20746d66); // "fmt "
        write_u32_le(out, 16);         // chunk size
        write_u16_le(out, 1);          // audio format
        write_u16_le(out, num_channels);
        write_u32_le(out, sample_rate);
        write_u32_le(out, 0);                                                           // byte_rate = 0 (invalid)
        write_u16_le(out, static_cast<uint16_t>(num_channels * (bits_per_sample / 8))); // block align
        write_u16_le(out, bits_per_sample);

        write_data_chunk(out, data);
        out.close();

        auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
        REQUIRE(ad.sample_rate == sample_rate);
        REQUIRE(ad.bit_rate == bits_per_sample);
        REQUIRE(ad.num_channels == num_channels);
        REQUIRE(ad.samples.size() == data.size());
    }

    SECTION("Odd-sized fmt chunk (19 bytes) with multiple extra bytes") {
        TempFile      tmp(make_temp_path("temp_fmt_odd19.wav"));
        std::ofstream out(tmp.path(), std::ios::binary);
        REQUIRE(out);

        uint16_t             num_channels    = 2;
        uint32_t             sample_rate     = 48000;
        uint16_t             bits_per_sample = 16;
        std::vector<uint8_t> data            = {0xAA, 0xBB, 0xCC, 0xDD};
        uint32_t             fmt_size        = 19; // odd with multiple extra bytes

        uint32_t riff_size = 4 + (8 + fmt_size) + (8 + static_cast<uint32_t>(data.size()));
        write_riff_header(out, riff_size);
        write_fmt_chunk(out, fmt_size, 1, num_channels, sample_rate, bits_per_sample);
        out.put(static_cast<char>(0x01)); // three extra bytes
        out.put(static_cast<char>(0x02));
        out.put(static_cast<char>(0x03));
        write_data_chunk(out, data);
        out.close();

        auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
        REQUIRE(ad.sample_rate == sample_rate);
        REQUIRE(ad.bit_rate == bits_per_sample);
        REQUIRE(ad.num_channels == num_channels);
        REQUIRE(ad.samples.size() == data.size());
    }
}

TEST_CASE("WAV parsing: odd-sized JUNK chunk with padding after fmt", "[wav][parsing][junk][padding]") {
    TempFile      tmp(make_temp_path("temp_odd_junk.wav"));
    std::ofstream out(tmp.path(), std::ios::binary);
    REQUIRE(out);

    uint16_t             num_channels    = 1;
    uint32_t             sample_rate     = 22050;
    uint16_t             bits_per_sample = 16;
    std::vector<uint8_t> data            = {0xAA, 0xBB, 0xCC, 0xDD};
    uint32_t             junk_size       = 3; // odd-sized

    // Calculate RIFF size including padding for odd-sized JUNK chunk
    uint32_t riff_size = 4 + (8 + 16) + (8 + junk_size + 1) + (8 + static_cast<uint32_t>(data.size()));

    write_u32_le(out, 0x46464952); // "RIFF"
    write_u32_le(out, riff_size);
    write_u32_le(out, 0x45564157); // "WAVE"

    write_fmt_chunk(out, 16, 1, num_channels, sample_rate, bits_per_sample);
    write_junk_chunk(out, junk_size, true); // with padding
    write_data_chunk(out, data);
    out.close();

    // Parser should skip JUNK chunk with padding and find data chunk
    auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
    REQUIRE(ad.sample_rate == sample_rate);
    REQUIRE(ad.bit_rate == bits_per_sample);
    REQUIRE(ad.num_channels == num_channels);
    REQUIRE(ad.samples.size() == data.size());
}

TEST_CASE("WAV parsing: JUNK chunk before fmt chunk", "[wav][parsing][junk][chunk_order]") {
    TempFile      tmp(make_temp_path("temp_junk_before_fmt.wav"));
    std::ofstream out(tmp.path(), std::ios::binary);
    REQUIRE(out);

    uint16_t             num_channels    = 1;
    uint32_t             sample_rate     = 44100;
    uint16_t             bits_per_sample = 16;
    std::vector<uint8_t> data            = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    uint32_t             junk_size       = 8; // even size

    // Calculate RIFF size: 4 (WAVE) + 8+junk_size + 8+16 (fmt) + 8+data_size
    uint32_t riff_size = 4 + (8 + junk_size) + (8 + 16) + (8 + static_cast<uint32_t>(data.size()));

    write_u32_le(out, 0x46464952); // "RIFF"
    write_u32_le(out, riff_size);
    write_u32_le(out, 0x45564157); // "WAVE"

    write_junk_chunk(out, junk_size, false); // JUNK BEFORE fmt (non-standard order)
    write_fmt_chunk(out, 16, 1, num_channels, sample_rate, bits_per_sample);
    write_data_chunk(out, data);
    out.close();

    // Parser should skip JUNK chunk and successfully find fmt and data chunks
    auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
    REQUIRE(ad.sample_rate == sample_rate);
    REQUIRE(ad.bit_rate == bits_per_sample);
    REQUIRE(ad.num_channels == num_channels);
    REQUIRE(ad.samples.size() == data.size());

    // Verify actual sample data
    for (size_t i = 0; i < data.size(); i++) {
        REQUIRE(ad.samples[i] == data[i]);
    }
}

TEST_CASE("WAV parsing: odd-sized JUNK chunk before fmt chunk with padding", "[wav][parsing][junk][chunk_order][padding]") {
    TempFile      tmp(make_temp_path("temp_junk_odd_before_fmt.wav"));
    std::ofstream out(tmp.path(), std::ios::binary);
    REQUIRE(out);

    uint16_t             num_channels    = 2; // Stereo for variety
    uint32_t             sample_rate     = 48000;
    uint16_t             bits_per_sample = 16;
    std::vector<uint8_t> data            = {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
    uint32_t             junk_size       = 5; // odd-sized

    // Calculate RIFF size: 4 (WAVE) + 8+junk_size+1 (JUNK with padding) + 8+16 (fmt) + 8+data_size
    uint32_t riff_size = 4 + (8 + junk_size + 1) + (8 + 16) + (8 + static_cast<uint32_t>(data.size()));

    write_u32_le(out, 0x46464952); // "RIFF"
    write_u32_le(out, riff_size);
    write_u32_le(out, 0x45564157); // "WAVE"

    write_junk_chunk(out, junk_size, true); // Odd-sized JUNK BEFORE fmt with padding
    write_fmt_chunk(out, 16, 1, num_channels, sample_rate, bits_per_sample);
    write_data_chunk(out, data);
    out.close();

    // Parser should skip odd-sized JUNK chunk (with padding) and successfully find fmt and data chunks
    auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
    REQUIRE(ad.sample_rate == sample_rate);
    REQUIRE(ad.bit_rate == bits_per_sample);
    REQUIRE(ad.num_channels == num_channels);
    REQUIRE(ad.samples.size() == data.size());

    // Verify actual sample data
    for (size_t i = 0; i < data.size(); i++) {
        REQUIRE(ad.samples[i] == data[i]);
    }
}

TEST_CASE("WAV parsing: truncated file throws", "[wav][parsing][error][truncated]") {
    TempFile      tmp(make_temp_path("temp_truncated.wav"));
    std::ofstream out(tmp.path(), std::ios::binary);
    REQUIRE(out);

    // Write deliberately truncated RIFF header (incomplete WAVE)
    write_u32_le(out, 0x46464952); // "RIFF"
    write_u32_le(out, 0);          // size
    out.write("WA", 2);            // incomplete 'WAVE'
    out.close();

    // Parser should throw when encountering truncated file
    REQUIRE_THROWS(AudioIndex::extractAudioDataFromAudioFile(tmp.path()));
}

TEST_CASE("WAV parsing: unsupported bitsPerSample throws", "[wav][parsing][error][validation]") {
    TempFile      tmp(make_temp_path("temp_unsupported_bps.wav"));
    std::ofstream out(tmp.path(), std::ios::binary);
    REQUIRE(out);

    uint16_t             num_channels    = 1;
    uint32_t             sample_rate     = 44100;
    uint16_t             bits_per_sample = 7; // unsupported
    std::vector<uint8_t> data            = {0x01, 0x02};

    uint32_t riff_size = 4 + (8 + 16) + (8 + static_cast<uint32_t>(data.size()));
    write_riff_header(out, riff_size);
    write_fmt_chunk(out, 16, 1, num_channels, sample_rate, bits_per_sample);
    write_data_chunk(out, data);
    out.close();

    // Parser should throw on unsupported bit depth
    REQUIRE_THROWS(AudioIndex::extractAudioDataFromAudioFile(tmp.path()));
}

TEST_CASE("WAV parsing: malformed headers throw", "[wav][parsing][error][validation]") {
    SECTION("bits_per_sample == 0 throws") {
        TempFile      tmp(make_temp_path("temp_malformed_bps0.wav"));
        std::ofstream out(tmp.path(), std::ios::binary);
        REQUIRE(out);

        std::vector<uint8_t> data            = {0x01, 0x02};
        uint32_t             riff_size       = 4 + (8 + 16) + (8 + static_cast<uint32_t>(data.size()));
        uint16_t             bits_per_sample = 0; // malformed

        write_riff_header(out, riff_size);
        write_fmt_chunk(out, 16, 1, 1, 44100, bits_per_sample);
        write_data_chunk(out, data);
        out.close();

        REQUIRE_THROWS(AudioIndex::extractAudioDataFromAudioFile(tmp.path()));
    }

    SECTION("num_channels == 0 throws") {
        TempFile      tmp(make_temp_path("temp_malformed_nc0.wav"));
        std::ofstream out(tmp.path(), std::ios::binary);
        REQUIRE(out);

        std::vector<uint8_t> data         = {0x01, 0x02};
        uint32_t             riff_size    = 4 + (8 + 16) + (8 + static_cast<uint32_t>(data.size()));
        uint16_t             num_channels = 0; // malformed

        write_riff_header(out, riff_size);
        write_fmt_chunk(out, 16, 1, num_channels, 44100, 16);
        write_data_chunk(out, data);
        out.close();

        REQUIRE_THROWS(AudioIndex::extractAudioDataFromAudioFile(tmp.path()));
    }

    SECTION("Missing fmt chunk throws") {
        TempFile      tmp(make_temp_path("temp_malformed_no_fmt.wav"));
        std::ofstream out(tmp.path(), std::ios::binary);
        REQUIRE(out);

        std::vector<uint8_t> data      = {0xDE, 0xAD, 0xBE, 0xEF};
        uint32_t             riff_size = 4 + (8 + static_cast<uint32_t>(data.size()));

        write_riff_header(out, riff_size);
        write_data_chunk(out, data); // No fmt chunk!
        out.close();

        REQUIRE_THROWS(AudioIndex::extractAudioDataFromAudioFile(tmp.path()));
    }
}

TEST_CASE("WAV parsing: fmt chunk too small throws", "[wav][parsing][error][validation]") {
    TempFile      tmp(make_temp_path("temp_fmt_small.wav"));
    std::ofstream out(tmp.path(), std::ios::binary);
    REQUIRE(out);

    // Write RIFF and a fmt chunk with size 10 (<16 minimum), no data chunk
    write_u32_le(out, 0x46464952); // "RIFF"
    write_u32_le(out, 0);          // size
    write_u32_le(out, 0x45564157); // "WAVE"
    write_u32_le(out, 0x20746d66); // "fmt "
    write_u32_le(out, 10);         // fmt size too small
    for (int i = 0; i < 10; ++i) {
        out.put(static_cast<char>(i));
    }
    out.close();

    // Parser should throw because fmt chunk is too small (and data chunk missing)
    REQUIRE_THROWS(AudioIndex::extractAudioDataFromAudioFile(tmp.path()));
}

TEST_CASE("WAV parsing: data chunk declared larger than actual throws", "[wav][parsing][error][validation]") {
    TempFile      tmp(make_temp_path("temp_data_mismatch.wav"));
    std::ofstream out(tmp.path(), std::ios::binary);
    REQUIRE(out);

    std::vector<uint8_t> data          = {0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t             declared_size = 10; // declare larger than actual
    uint32_t             riff_size     = 4 + (8 + 16) + (8 + declared_size);

    write_riff_header(out, riff_size);
    write_fmt_chunk(out, 16, 1, 1, 8000, 16);
    write_data_chunk_with_size(out, data, declared_size); // Declared size larger than actual
    out.close();

    // Parser should throw when data chunk size doesn't match actual file content
    REQUIRE_THROWS(AudioIndex::extractAudioDataFromAudioFile(tmp.path()));
}
