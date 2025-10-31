/**
 * @file test_wav_parsing.cpp
 * @brief Unit tests for WAV file parsing, header validation, and edge cases.
 *
 * Extracted from the original test_main.cpp (lines ~1293-2017).
 * Covers:
 *  - WAV header correctness on export
 *  - fmt chunk variants (extra/odd bytes)
 *  - odd-sized unknown chunk with padding
 *  - truncated/malformed files
 *  - unsupported bit depths
 *  - declared data chunk larger than actual
 */

#include <AudioIndex.h>

#include <fstream>
#include <string>
#include <vector>

#include "test_common.h"


using namespace AudioBabel;

void register_wav_parsing_tests(TestRunner& runner) {
    // ---- WAV header correctness ----
    runner.add("AudioIndex: exportAudioDataToWav header correctness", [&runner]() -> bool {
        const std::string name = "AudioIndex: exportAudioDataToWav header correctness";
        bool              ok   = true;
        try {
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

            std::ifstream in(tmp.path(), std::ios::binary);
            if (!in) {
                runner.failMsg(name, "failed to open written WAV file");
                return false;
            }
            std::vector<uint8_t> hdr(44);
            in.read(reinterpret_cast<char*>(hdr.data()), static_cast<std::streamsize>(hdr.size()));
            if (!in) {
                runner.failMsg(name, "failed to read WAV header bytes");
                return false;
            }

            ok &= RUN_CHECK(runner, name, hdr[0] == 'R' && hdr[1] == 'I' && hdr[2] == 'F' && hdr[3] == 'F', "RIFF tag");
            uint32_t file_size = static_cast<uint32_t>(hdr[4]) | (static_cast<uint32_t>(hdr[5]) << 8) | (static_cast<uint32_t>(hdr[6]) << 16) |
                                 (static_cast<uint32_t>(hdr[7]) << 24);
            uint32_t expected_file_size = 36U + static_cast<uint32_t>(audioData.samples.size());
            ok &= RUN_CHECK(runner, name, file_size == expected_file_size, "file size matches expected (36 + data bytes)");

            ok &= RUN_CHECK(runner, name, hdr[8] == 'W' && hdr[9] == 'A' && hdr[10] == 'V' && hdr[11] == 'E', "WAVE tag");
            ok &= RUN_CHECK(runner, name, hdr[12] == 'f' && hdr[13] == 'm' && hdr[14] == 't' && hdr[15] == ' ', "fmt chunk id");

            uint32_t fmt_size = static_cast<uint32_t>(hdr[16]) | (static_cast<uint32_t>(hdr[17]) << 8) | (static_cast<uint32_t>(hdr[18]) << 16) |
                                (static_cast<uint32_t>(hdr[19]) << 24);
            ok &= RUN_CHECK(runner, name, fmt_size == 16U, "fmt chunk size == 16");

            uint16_t audio_format = static_cast<uint16_t>(hdr[20]) | (static_cast<uint16_t>(hdr[21]) << 8);
            ok &= RUN_CHECK(runner, name, audio_format == audioData.audio_format, "audio format matches (PCM=1)");

            uint16_t num_channels = static_cast<uint16_t>(hdr[22]) | (static_cast<uint16_t>(hdr[23]) << 8);
            ok &= RUN_CHECK(runner, name, num_channels == audioData.num_channels, "num channels matches");

            uint32_t sample_rate = static_cast<uint32_t>(hdr[24]) | (static_cast<uint32_t>(hdr[25]) << 8) | (static_cast<uint32_t>(hdr[26]) << 16) |
                                   (static_cast<uint32_t>(hdr[27]) << 24);
            ok &= RUN_CHECK(runner, name, sample_rate == audioData.sample_rate, "sample rate matches");

            uint16_t bits_per_sample = static_cast<uint16_t>(hdr[34]) | (static_cast<uint16_t>(hdr[35]) << 8);
            ok &= RUN_CHECK(runner, name, bits_per_sample == audioData.bit_rate, "bits per sample matches bit_rate");

            ok &= RUN_CHECK(runner, name, hdr[36] == 'd' && hdr[37] == 'a' && hdr[38] == 't' && hdr[39] == 'a', "data chunk id");
            uint32_t data_size = static_cast<uint32_t>(hdr[40]) | (static_cast<uint32_t>(hdr[41]) << 8) | (static_cast<uint32_t>(hdr[42]) << 16) |
                                 (static_cast<uint32_t>(hdr[43]) << 24);
            ok &= RUN_CHECK(runner, name, data_size == static_cast<uint32_t>(audioData.samples.size()), "data chunk size matches samples size");

            // cleanup handled by TempFile destructor

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    // WAV edge-case tests: fmt chunk with extra bytes, odd-sized unknown chunk, and truncated file
    runner.add("AudioIndex: wav fmt chunk with extra bytes", [&runner]() -> bool {
        const std::string name = "AudioIndex: wav fmt chunk with extra bytes";
        bool              ok   = true;
        TempFile          tmp(make_temp_path("temp_fmt_extra.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }

            // Parameters
            uint16_t audio_format    = 1;
            uint16_t num_channels    = 1;
            uint32_t sample_rate     = 44100;
            uint16_t bits_per_sample = 16;
            uint32_t byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

            // payload
            std::vector<uint8_t> data      = {0x11, 0x22, 0x33, 0x44};
            auto                 data_size = static_cast<uint32_t>(data.size());

            uint32_t fmt_size = 18; // 2 extra bytes beyond canonical 16

            // Compute RIFF size = 4 (WAVE) + (8 + fmt_size) + (8 + data_size)
            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

            // write RIFF header
            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            // fmt chunk
            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

            // 16 canonical bytes
            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            // extra two bytes
            out.put(static_cast<char>(0x55));
            out.put(static_cast<char>(0x66));

            // data chunk
            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            // call extractor
            auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            ok &= RUN_CHECK(runner, name, ad.sample_rate == sample_rate, "sample rate matches");
            ok &= RUN_CHECK(runner, name, ad.bit_rate == bits_per_sample, "bit depth matches");
            ok &= RUN_CHECK(runner, name, ad.num_channels == num_channels, "num channels matches");
            ok &= RUN_CHECK(runner, name, ad.samples.size() == data_size, "data size matches");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        // cleanup handled by TempFile destructor
        return ok;
    });

    runner.add("AudioIndex: wav fmt variants (odd size, invalid byte_rate, extra bytes)", [&runner]() -> bool {
        const std::string name = "AudioIndex: wav fmt variants (odd size, invalid byte_rate, extra bytes)";
        bool              ok   = true;

        // Case A: fmt chunk odd size (17) with one extra byte - should be tolerated
        try {
            TempFile      tmp(make_temp_path("temp_fmt_odd17.wav"));
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav (fmt odd 17)");
                return false;
            }
            uint16_t             audio_format    = 1;
            uint16_t             num_channels    = 1;
            uint32_t             sample_rate     = 44100;
            uint16_t             bits_per_sample = 16;
            uint32_t             byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto                 block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));
            std::vector<uint8_t> data            = {0x11, 0x22};
            auto                 data_size       = static_cast<uint32_t>(data.size());

            uint32_t fmt_size  = 17; // odd
            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

            // canonical 16 bytes
            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            // one extra byte (odd fmt)
            out.put(static_cast<char>(0x7F));

            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            ok &= RUN_CHECK(runner, name, ad.sample_rate == sample_rate, "fmt odd: sample rate matches");
            ok &= RUN_CHECK(runner, name, ad.bit_rate == bits_per_sample, "fmt odd: bit depth matches");
            ok &= RUN_CHECK(runner, name, ad.num_channels == num_channels, "fmt odd: num channels matches");
            ok &= RUN_CHECK(runner, name, ad.samples.size() == data_size, "fmt odd: data size matches");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception (fmt odd): ") + e.what());
            ok = false;
        }

        // Case B: invalid byte_rate (zero) - extractor should not crash and should parse other fields
        try {
            TempFile      tmp(make_temp_path("temp_fmt_byte_rate0.wav"));
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav (byte_rate==0)");
                return false;
            }
            uint16_t             audio_format    = 1;
            uint16_t             num_channels    = 1;
            uint32_t             sample_rate     = 22050;
            uint16_t             bits_per_sample = 16;
            uint32_t             byte_rate       = 0; // invalid
            auto                 block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));
            std::vector<uint8_t> data            = {0x55, 0x66, 0x77};
            auto                 data_size       = static_cast<uint32_t>(data.size());

            uint32_t fmt_size  = 16;
            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            ok &= RUN_CHECK(runner, name, ad.sample_rate == sample_rate, "byte_rate0: sample rate matches");
            ok &= RUN_CHECK(runner, name, ad.bit_rate == bits_per_sample, "byte_rate0: bit depth matches");
            ok &= RUN_CHECK(runner, name, ad.num_channels == num_channels, "byte_rate0: num channels matches");
            ok &= RUN_CHECK(runner, name, ad.samples.size() == data_size, "byte_rate0: data size matches");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception (byte_rate0): ") + e.what());
            ok = false;
        }

        // Case C: fmt chunk odd size with multiple extra bytes (19) - should be tolerated
        try {
            TempFile      tmp(make_temp_path("temp_fmt_odd19.wav"));
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav (fmt odd 19)");
                return false;
            }
            uint16_t             audio_format    = 1;
            uint16_t             num_channels    = 2;
            uint32_t             sample_rate     = 48000;
            uint16_t             bits_per_sample = 16;
            uint32_t             byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto                 block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));
            std::vector<uint8_t> data            = {0xAA, 0xBB, 0xCC, 0xDD};
            auto                 data_size       = static_cast<uint32_t>(data.size());

            uint32_t fmt_size  = 19; // odd with multiple extra bytes
            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            // three extra bytes
            out.put(static_cast<char>(0x01));
            out.put(static_cast<char>(0x02));
            out.put(static_cast<char>(0x03));

            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            ok &= RUN_CHECK(runner, name, ad.sample_rate == sample_rate, "fmt odd19: sample rate matches");
            ok &= RUN_CHECK(runner, name, ad.bit_rate == bits_per_sample, "fmt odd19: bit depth matches");
            ok &= RUN_CHECK(runner, name, ad.num_channels == num_channels, "fmt odd19: num channels matches");
            ok &= RUN_CHECK(runner, name, ad.samples.size() == data_size, "fmt odd19: data size matches");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception (fmt odd19): ") + e.what());
            ok = false;
        }

        return ok;
    });

    runner.add("AudioIndex: wav odd-sized unknown chunk with padding", [&runner]() -> bool {
        const std::string name = "AudioIndex: wav odd-sized unknown chunk with padding";
        bool              ok   = true;
        TempFile          tmp(make_temp_path("temp_odd_junk.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }

            uint16_t audio_format    = 1;
            uint16_t num_channels    = 1;
            uint32_t sample_rate     = 22050;
            uint16_t bits_per_sample = 16;
            uint32_t byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

            std::vector<uint8_t> data      = {0xAA, 0xBB, 0xCC, 0xDD};
            auto                 data_size = static_cast<uint32_t>(data.size());

            uint32_t fmt_size = 16;
            // unknown chunk size odd (3)
            uint32_t junk_size = 3;

            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + junk_size + 1) + (8 + data_size); // include pad byte for junk

            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            // fmt chunk
            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));
            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            // JUNK chunk (odd length)
            out.write("JUNK", 4);
            out.put(static_cast<char>(junk_size & 0xFF));
            out.put(static_cast<char>((junk_size >> 8) & 0xFF));
            out.put(static_cast<char>((junk_size >> 16) & 0xFF));
            out.put(static_cast<char>((junk_size >> 24) & 0xFF));
            // 3 bytes of junk
            out.put(static_cast<char>(0x01));
            out.put(static_cast<char>(0x02));
            out.put(static_cast<char>(0x03));
            // pad byte because chunk size is odd
            out.put(static_cast<char>(0x00));

            // data chunk
            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            ok &= RUN_CHECK(runner, name, ad.sample_rate == sample_rate, "sample rate matches");
            ok &= RUN_CHECK(runner, name, ad.bit_rate == bits_per_sample, "bit depth matches");
            ok &= RUN_CHECK(runner, name, ad.num_channels == num_channels, "num channels matches");
            ok &= RUN_CHECK(runner, name, ad.samples.size() == data_size, "data size matches");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        // cleanup handled by TempFile destructor
        return ok;
    });

    runner.add("AudioIndex: wav JUNK chunk before fmt chunk", [&runner]() -> bool {
        const std::string name = "AudioIndex: wav JUNK chunk before fmt chunk";
        bool              ok   = true;
        TempFile          tmp(make_temp_path("temp_junk_before_fmt.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }

            uint16_t audio_format    = 1;
            uint16_t num_channels    = 1;
            uint32_t sample_rate     = 44100;
            uint16_t bits_per_sample = 16;
            uint32_t byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

            std::vector<uint8_t> data      = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
            auto                 data_size = static_cast<uint32_t>(data.size());

            uint32_t fmt_size = 16;
            // JUNK chunk before fmt: even size for simplicity (8 bytes of junk data)
            uint32_t junk_size = 8;

            // Calculate RIFF size: 4 (WAVE) + 8+junk_size (JUNK chunk) + 8+fmt_size (fmt) + 8+data_size (data)
            uint32_t riff_size = 4 + (8 + junk_size) + (8 + fmt_size) + (8 + data_size);

            // Write RIFF header
            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            // JUNK chunk BEFORE fmt chunk
            out.write("JUNK", 4);
            out.put(static_cast<char>(junk_size & 0xFF));
            out.put(static_cast<char>((junk_size >> 8) & 0xFF));
            out.put(static_cast<char>((junk_size >> 16) & 0xFF));
            out.put(static_cast<char>((junk_size >> 24) & 0xFF));
            // Write 8 bytes of arbitrary junk data
            for (uint32_t i = 0; i < junk_size; i++) {
                out.put(static_cast<char>(0xAA + i));
            }

            // fmt chunk
            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));
            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            // data chunk
            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            // Parser should skip JUNK chunk and successfully find fmt and data chunks
            auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            ok &= RUN_CHECK(runner, name, ad.sample_rate == sample_rate, "sample rate matches");
            ok &= RUN_CHECK(runner, name, ad.bit_rate == bits_per_sample, "bit depth matches");
            ok &= RUN_CHECK(runner, name, ad.num_channels == num_channels, "num channels matches");
            ok &= RUN_CHECK(runner, name, ad.samples.size() == data_size, "data size matches");

            // Verify actual sample data
            bool samples_match = true;
            for (size_t i = 0; i < data.size() && i < ad.samples.size(); i++) {
                if (ad.samples[i] != data[i]) {
                    samples_match = false;
                    break;
                }
            }
            ok &= RUN_CHECK(runner, name, samples_match, "sample data matches");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        // cleanup handled by TempFile destructor
        return ok;
    });

    runner.add("AudioIndex: wav odd-sized JUNK chunk before fmt chunk with padding", [&runner]() -> bool {
        const std::string name = "AudioIndex: wav odd-sized JUNK chunk before fmt chunk with padding";
        bool              ok   = true;
        TempFile          tmp(make_temp_path("temp_junk_odd_before_fmt.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }

            uint16_t audio_format    = 1;
            uint16_t num_channels    = 2; // Stereo for variety
            uint32_t sample_rate     = 48000;
            uint16_t bits_per_sample = 16;
            uint32_t byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

            std::vector<uint8_t> data      = {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
            auto                 data_size = static_cast<uint32_t>(data.size());

            uint32_t fmt_size = 16;
            // Odd-sized JUNK chunk before fmt (5 bytes of junk data)
            uint32_t junk_size = 5;

            // Calculate RIFF size: 4 (WAVE) + 8+junk_size+1 (JUNK chunk with padding) + 8+fmt_size (fmt) + 8+data_size (data)
            uint32_t riff_size = 4 + (8 + junk_size + 1) + (8 + fmt_size) + (8 + data_size);

            // Write RIFF header
            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            // Odd-sized JUNK chunk BEFORE fmt chunk
            out.write("JUNK", 4);
            out.put(static_cast<char>(junk_size & 0xFF));
            out.put(static_cast<char>((junk_size >> 8) & 0xFF));
            out.put(static_cast<char>((junk_size >> 16) & 0xFF));
            out.put(static_cast<char>((junk_size >> 24) & 0xFF));
            // Write 5 bytes of arbitrary junk data
            for (uint32_t i = 0; i < junk_size; i++) {
                out.put(static_cast<char>(0x11 * (i + 1)));
            }
            // Pad byte because chunk size is odd (RIFF requires even alignment)
            out.put(static_cast<char>(0x00));

            // fmt chunk
            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));
            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            // data chunk
            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            // Parser should skip odd-sized JUNK chunk (with padding) and successfully find fmt and data chunks
            auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
            ok &= RUN_CHECK(runner, name, ad.sample_rate == sample_rate, "sample rate matches");
            ok &= RUN_CHECK(runner, name, ad.bit_rate == bits_per_sample, "bit depth matches");
            ok &= RUN_CHECK(runner, name, ad.num_channels == num_channels, "num channels matches");
            ok &= RUN_CHECK(runner, name, ad.samples.size() == data_size, "data size matches");

            // Verify actual sample data
            bool samples_match = true;
            for (size_t i = 0; i < data.size() && i < ad.samples.size(); i++) {
                if (ad.samples[i] != data[i]) {
                    samples_match = false;
                    break;
                }
            }
            ok &= RUN_CHECK(runner, name, samples_match, "sample data matches with odd JUNK padding");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        // cleanup handled by TempFile destructor
        return ok;
    });

    runner.add("AudioIndex: wav truncated file throws", [&runner]() -> bool {
        const std::string name  = "AudioIndex: wav truncated file throws";
        bool              threw = false;
        TempFile          tmp(make_temp_path("temp_truncated.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }
            // write a deliberately truncated RIFF header (incomplete WAVE)
            out.write("RIFF", 4);
            out.put(static_cast<char>(0));
            out.put(static_cast<char>(0));
            out.put(static_cast<char>(0));
            out.put(static_cast<char>(0));
            out.write("WA", 2); // incomplete 'WAVE'
            out.close();

            try {
                auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
                (void) ad;
            } catch (const std::exception& e) {
                (void) e;
                threw = true;
            }
        } catch (...) {
            threw = true;
        }
        // cleanup handled by TempFile destructor
        return RUN_CHECK(runner, name, threw, "truncated wav should cause extractor to throw or fail");
    });

    // Additional negative WAV tests
    runner.add("AudioIndex: wav unsupported bitsPerSample throws", [&runner]() -> bool {
        const std::string name  = "AudioIndex: wav unsupported bitsPerSample throws";
        bool              threw = false;
        TempFile          tmp(make_temp_path("temp_unsupported_bps.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }

            uint16_t audio_format = 1;
            uint16_t num_channels = 1;
            uint32_t sample_rate  = 44100;
            // unsupported bits per sample (7)
            uint16_t bits_per_sample = 7;
            uint32_t byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

            std::vector<uint8_t> data      = {0x01, 0x02};
            auto                 data_size = static_cast<uint32_t>(data.size());

            // riff size
            uint32_t fmt_size  = 16;
            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            out.write("data", 4);
            out.put(static_cast<char>(data_size & 0xFF));
            out.put(static_cast<char>((data_size >> 8) & 0xFF));
            out.put(static_cast<char>((data_size >> 16) & 0xFF));
            out.put(static_cast<char>((data_size >> 24) & 0xFF));
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            out.close();

            try {
                auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
                (void) ad;
            } catch (const std::exception& e) {
                (void) e;
                threw = true;
            }
        } catch (...) {
            threw = true;
        }
        // cleanup handled by TempFile destructor
        return RUN_CHECK(runner, name, threw, "unsupported bitsPerSample should cause extractor to throw or fail");
    });

    runner.add("AudioIndex: wav malformed headers throw", [&runner]() -> bool {
        const std::string name = "AudioIndex: wav malformed headers throw";
        bool              ok   = true;

        // Case A: bits_per_sample == 0
        {
            TempFile tmp(make_temp_path("temp_malformed_bps0.wav"));
            try {
                std::ofstream out(tmp.path(), std::ios::binary);
                if (!out) {
                    runner.failMsg(name, "failed to create temp wav (bps==0)");
                    return false;
                }

                uint16_t audio_format    = 1;
                uint16_t num_channels    = 1;
                uint32_t sample_rate     = 44100;
                uint16_t bits_per_sample = 0; // malformed
                uint32_t byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
                auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

                std::vector<uint8_t> data      = {0x01, 0x02};
                auto                 data_size = static_cast<uint32_t>(data.size());

                uint32_t fmt_size  = 16;
                uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

                out.write("RIFF", 4);
                out.put(static_cast<char>(riff_size & 0xFF));
                out.put(static_cast<char>((riff_size >> 8) & 0xFF));
                out.put(static_cast<char>((riff_size >> 16) & 0xFF));
                out.put(static_cast<char>((riff_size >> 24) & 0xFF));
                out.write("WAVE", 4);

                out.write("fmt ", 4);
                out.put(static_cast<char>(fmt_size & 0xFF));
                out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
                out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
                out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

                out.put(static_cast<char>(audio_format & 0xFF));
                out.put(static_cast<char>((audio_format >> 8) & 0xFF));
                out.put(static_cast<char>(num_channels & 0xFF));
                out.put(static_cast<char>((num_channels >> 8) & 0xFF));
                out.put(static_cast<char>(sample_rate & 0xFF));
                out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
                out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
                out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
                out.put(static_cast<char>(byte_rate & 0xFF));
                out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
                out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
                out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
                out.put(static_cast<char>(block_align & 0xFF));
                out.put(static_cast<char>((block_align >> 8) & 0xFF));
                out.put(static_cast<char>(bits_per_sample & 0xFF));
                out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

                out.write("data", 4);
                out.put(static_cast<char>(data_size & 0xFF));
                out.put(static_cast<char>((data_size >> 8) & 0xFF));
                out.put(static_cast<char>((data_size >> 16) & 0xFF));
                out.put(static_cast<char>((data_size >> 24) & 0xFF));
                out.write(reinterpret_cast<const char*>(data.data()), data.size());
                out.close();

                bool threw = false;
                try {
                    auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
                    (void) ad;
                } catch (...) {
                    threw = true;
                }
                ok &= RUN_CHECK(runner, name, threw, "bits_per_sample == 0 should cause extractor to throw");
            } catch (const std::exception& e) {
                runner.failMsg(name, std::string("exception: ") + e.what());
                ok = false;
            }
        }

        // Case B: num_channels == 0
        {
            TempFile tmp(make_temp_path("temp_malformed_nc0.wav"));
            try {
                std::ofstream out(tmp.path(), std::ios::binary);
                if (!out) {
                    runner.failMsg(name, "failed to create temp wav (nc==0)");
                    return false;
                }

                uint16_t audio_format    = 1;
                uint16_t num_channels    = 0; // malformed
                uint32_t sample_rate     = 44100;
                uint16_t bits_per_sample = 16;
                uint32_t byte_rate       = sample_rate * (num_channels) * (bits_per_sample / 8);
                auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

                std::vector<uint8_t> data      = {0x01, 0x02};
                auto                 data_size = static_cast<uint32_t>(data.size());

                uint32_t fmt_size  = 16;
                uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

                out.write("RIFF", 4);
                out.put(static_cast<char>(riff_size & 0xFF));
                out.put(static_cast<char>((riff_size >> 8) & 0xFF));
                out.put(static_cast<char>((riff_size >> 16) & 0xFF));
                out.put(static_cast<char>((riff_size >> 24) & 0xFF));
                out.write("WAVE", 4);

                out.write("fmt ", 4);
                out.put(static_cast<char>(fmt_size & 0xFF));
                out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
                out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
                out.put(static_cast<char>((fmt_size >> 24) & 0xFF));

                out.put(static_cast<char>(audio_format & 0xFF));
                out.put(static_cast<char>((audio_format >> 8) & 0xFF));
                out.put(static_cast<char>(num_channels & 0xFF));
                out.put(static_cast<char>((num_channels >> 8) & 0xFF));
                out.put(static_cast<char>(sample_rate & 0xFF));
                out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
                out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
                out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
                out.put(static_cast<char>(byte_rate & 0xFF));
                out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
                out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
                out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
                out.put(static_cast<char>(block_align & 0xFF));
                out.put(static_cast<char>((block_align >> 8) & 0xFF));
                out.put(static_cast<char>(bits_per_sample & 0xFF));
                out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

                out.write("data", 4);
                out.put(static_cast<char>(data_size & 0xFF));
                out.put(static_cast<char>((data_size >> 8) & 0xFF));
                out.put(static_cast<char>((data_size >> 16) & 0xFF));
                out.put(static_cast<char>((data_size >> 24) & 0xFF));
                out.write(reinterpret_cast<const char*>(data.data()), data.size());
                out.close();

                bool threw = false;
                try {
                    auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
                    (void) ad;
                } catch (...) {
                    threw = true;
                }
                ok &= RUN_CHECK(runner, name, threw, "num_channels == 0 should cause extractor to throw");
            } catch (const std::exception& e) {
                runner.failMsg(name, std::string("exception: ") + e.what());
                ok = false;
            }
        }

        // Case C: missing fmt chunk (only data chunk present)
        {
            TempFile tmp(make_temp_path("temp_malformed_no_fmt.wav"));
            try {
                std::ofstream out(tmp.path(), std::ios::binary);
                if (!out) {
                    runner.failMsg(name, "failed to create temp wav (no fmt)");
                    return false;
                }

                std::vector<uint8_t> data      = {0xDE, 0xAD, 0xBE, 0xEF};
                auto                 data_size = static_cast<uint32_t>(data.size());
                uint32_t             riff_size = 4 + (8 + data_size);

                out.write("RIFF", 4);
                out.put(static_cast<char>(riff_size & 0xFF));
                out.put(static_cast<char>((riff_size >> 8) & 0xFF));
                out.put(static_cast<char>((riff_size >> 16) & 0xFF));
                out.put(static_cast<char>((riff_size >> 24) & 0xFF));
                out.write("WAVE", 4);

                out.write("data", 4);
                out.put(static_cast<char>(data_size & 0xFF));
                out.put(static_cast<char>((data_size >> 8) & 0xFF));
                out.put(static_cast<char>((data_size >> 16) & 0xFF));
                out.put(static_cast<char>((data_size >> 24) & 0xFF));
                out.write(reinterpret_cast<const char*>(data.data()), data.size());
                out.close();

                bool threw = false;
                try {
                    auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
                    (void) ad;
                } catch (...) {
                    threw = true;
                }
                ok &= RUN_CHECK(runner, name, threw, "missing fmt chunk should cause extractor to throw");
            } catch (const std::exception& e) {
                runner.failMsg(name, std::string("exception: ") + e.what());
                ok = false;
            }
        }

        return ok;
    });

    runner.add("AudioIndex: wav fmt chunk too small throws", [&runner]() -> bool {
        const std::string name  = "AudioIndex: wav fmt chunk too small throws";
        bool              threw = false;
        TempFile          tmp(make_temp_path("temp_fmt_small.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }

            // write RIFF and a fmt chunk with size 10 (<16), no data chunk
            out.write("RIFF", 4);
            out.put(static_cast<char>(0));
            out.put(static_cast<char>(0));
            out.put(static_cast<char>(0));
            out.put(static_cast<char>(0));
            out.write("WAVE", 4);
            out.write("fmt ", 4);
            uint32_t small = 10;
            out.put(static_cast<char>(small & 0xFF));
            out.put(static_cast<char>((small >> 8) & 0xFF));
            out.put(static_cast<char>((small >> 16) & 0xFF));
            out.put(static_cast<char>((small >> 24) & 0xFF));
            // write 10 arbitrary bytes to satisfy the small chunk
            for (int i = 0; i < 10; ++i) {
                out.put(static_cast<char>(i));
            }
            out.close();

            try {
                auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
                (void) ad;
            } catch (const std::exception& e) {
                (void) e;
                threw = true;
            }
        } catch (...) {
            threw = true;
        }
        // cleanup handled by TempFile destructor
        return RUN_CHECK(runner, name, threw, "fmt chunk too small should cause extractor to fail/throw because no data chunk will be found");
    });

    runner.add("AudioIndex: wav data chunk declared larger than actual throws", [&runner]() -> bool {
        const std::string name  = "AudioIndex: wav data chunk declared larger than actual throws";
        bool              threw = false;
        TempFile          tmp(make_temp_path("temp_data_mismatch.wav"));
        try {
            std::ofstream out(tmp.path(), std::ios::binary);
            if (!out) {
                runner.failMsg(name, "failed to create temp wav");
                return false;
            }

            uint16_t audio_format    = 1;
            uint16_t num_channels    = 1;
            uint32_t sample_rate     = 8000;
            uint16_t bits_per_sample = 16;
            uint32_t byte_rate       = sample_rate * num_channels * (bits_per_sample / 8);
            auto     block_align     = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

            std::vector<uint8_t> data          = {0xDE, 0xAD, 0xBE, 0xEF};
            uint32_t             declared_size = 10; // declare larger than actual
            auto                 actual_size   = static_cast<uint32_t>(data.size());

            uint32_t fmt_size  = 16;
            uint32_t riff_size = 4 + (8 + fmt_size) + (8 + declared_size);

            out.write("RIFF", 4);
            out.put(static_cast<char>(riff_size & 0xFF));
            out.put(static_cast<char>((riff_size >> 8) & 0xFF));
            out.put(static_cast<char>((riff_size >> 16) & 0xFF));
            out.put(static_cast<char>((riff_size >> 24) & 0xFF));
            out.write("WAVE", 4);

            out.write("fmt ", 4);
            out.put(static_cast<char>(fmt_size & 0xFF));
            out.put(static_cast<char>((fmt_size >> 8) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 16) & 0xFF));
            out.put(static_cast<char>((fmt_size >> 24) & 0xFF));
            out.put(static_cast<char>(audio_format & 0xFF));
            out.put(static_cast<char>((audio_format >> 8) & 0xFF));
            out.put(static_cast<char>(num_channels & 0xFF));
            out.put(static_cast<char>((num_channels >> 8) & 0xFF));
            out.put(static_cast<char>(sample_rate & 0xFF));
            out.put(static_cast<char>((sample_rate >> 8) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 16) & 0xFF));
            out.put(static_cast<char>((sample_rate >> 24) & 0xFF));
            out.put(static_cast<char>(byte_rate & 0xFF));
            out.put(static_cast<char>((byte_rate >> 8) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 16) & 0xFF));
            out.put(static_cast<char>((byte_rate >> 24) & 0xFF));
            out.put(static_cast<char>(block_align & 0xFF));
            out.put(static_cast<char>((block_align >> 8) & 0xFF));
            out.put(static_cast<char>(bits_per_sample & 0xFF));
            out.put(static_cast<char>((bits_per_sample >> 8) & 0xFF));

            out.write("data", 4);
            out.put(static_cast<char>(declared_size & 0xFF));
            out.put(static_cast<char>((declared_size >> 8) & 0xFF));
            out.put(static_cast<char>((declared_size >> 16) & 0xFF));
            out.put(static_cast<char>((declared_size >> 24) & 0xFF));
            // write only actual_size bytes
            out.write(reinterpret_cast<const char*>(data.data()), actual_size);
            out.close();

            try {
                auto ad = AudioIndex::extractAudioDataFromAudioFile(tmp.path());
                (void) ad;
            } catch (const std::exception& e) {
                (void) e;
                threw = true;
            }

        } catch (...) {
            threw = true;
        }
        // cleanup handled by TempFile destructor
        return RUN_CHECK(runner, name, threw, "declared data chunk larger than actual should cause extractor to fail/throw");
    });
}
