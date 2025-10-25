/**
 * sample_base64_cli.cpp
 * ---------------------
 * Command-line tool for converting audio files to/from sample-based base64 encoding.
 * 
 * This tool demonstrates the alternate index format where each 16-bit audio sample
 * is represented by exactly 3 base64 characters using simple base-64 number conversion,
 * allowing for direct sample-level encoding and decoding.
 * 
 * Usage:
 *   sample_base64_cli encode <input.wav> <output.txt>
 *     Encode a 16-bit WAV file to sample-based base64 format
 * 
 *   sample_base64_cli decode <input.txt> <output.wav> [sample_rate] [channels]
 *     Decode a sample-based base64 file back to WAV format
 *     Optional: sample_rate (default 44100) and channels (default 1)
 * 
 * Example:
 *   sample_base64_cli encode audio.wav encoded.txt
 *   sample_base64_cli decode encoded.txt reconstructed.wav 44100 1
 */

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "AudioIndex.h"

using namespace AudioBabel;

void print_usage() {
    std::cout << "Usage:\n";
    std::cout << "  sample_base64_cli encode <input.wav> <output.txt>\n";
    std::cout << "    Encode a 16-bit WAV file to sample-based base64 format\n\n";
    std::cout << "  sample_base64_cli decode <input.txt> <output.wav> [sample_rate] [channels]\n";
    std::cout << "    Decode a sample-based base64 file back to WAV format\n";
    std::cout << "    Optional: sample_rate (default 44100) and channels (default 1)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sample_base64_cli encode audio.wav encoded.txt\n";
    std::cout << "  sample_base64_cli decode encoded.txt reconstructed.wav 44100 1\n";
}

void encode_audio(const std::string& input_wav, const std::string& output_txt) {
    try {
        std::cout << "Reading WAV file: " << input_wav << "\n";
        auto audioData = AudioIndex::extractAudioDataFromAudioFile(input_wav);

        // Verify it's 16-bit audio
        if (audioData.bit_rate != 16) {
            std::cerr << "Error: Input audio must be 16-bit. Got " << audioData.bit_rate << "-bit.\n";
            std::cerr << "Please convert your audio to 16-bit format first.\n";
            return;
        }

        std::cout << "Audio info:\n";
        std::cout << "  Sample rate: " << audioData.sample_rate << " Hz\n";
        std::cout << "  Bit depth: " << audioData.bit_rate << " bits\n";
        std::cout << "  Channels: " << audioData.num_channels << "\n";
        std::cout << "  Frames: " << audioData.num_frames << "\n";
        std::cout << "  Total samples: " << (audioData.num_frames * audioData.num_channels) << "\n";

        std::cout << "Encoding to sample-based base64...\n";
        std::string base64 = AudioIndex::audioDataToSampleBase64(audioData);

        std::cout << "Writing to: " << output_txt << "\n";
        std::ofstream out(output_txt);
        if (!out) {
            std::cerr << "Error: Could not open output file: " << output_txt << "\n";
            return;
        }

        out << base64;
        out.close();

        std::cout << "Success! Encoded " << (audioData.num_frames * audioData.num_channels) << " samples to " << base64.length() << " characters\n";
        std::cout << "  (" << (base64.length() / 3) << " samples × 3 characters per sample)\n";

    } catch (const std::exception& e) {
        std::cerr << "Error during encoding: " << e.what() << "\n";
    }
}

void decode_audio(const std::string& input_txt, const std::string& output_wav, uint32_t sample_rate, uint16_t num_channels) {
    try {
        std::cout << "Reading base64 file: " << input_txt << "\n";
        std::ifstream in(input_txt);
        if (!in) {
            std::cerr << "Error: Could not open input file: " << input_txt << "\n";
            return;
        }

        std::string base64;
        std::getline(in, base64);
        in.close();

        std::cout << "Read " << base64.length() << " characters\n";
        std::cout << "  (" << (base64.length() / 3) << " samples expected)\n";

        std::cout << "Decoding from sample-based base64...\n";
        auto audioData = AudioIndex::sampleBase64ToAudioData(base64, sample_rate, num_channels);

        std::cout << "Decoded audio info:\n";
        std::cout << "  Sample rate: " << audioData.sample_rate << " Hz\n";
        std::cout << "  Bit depth: " << audioData.bit_rate << " bits\n";
        std::cout << "  Channels: " << audioData.num_channels << "\n";
        std::cout << "  Frames: " << audioData.num_frames << "\n";
        std::cout << "  Total samples: " << (audioData.num_frames * audioData.num_channels) << "\n";

        std::cout << "Writing WAV file: " << output_wav << "\n";
        AudioIndex::exportAudioDataToWav(audioData, output_wav);

        std::cout << "Success! Decoded " << (audioData.num_frames * audioData.num_channels) << " samples to WAV file\n";

    } catch (const std::exception& e) {
        std::cerr << "Error during decoding: " << e.what() << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        print_usage();
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "encode") {
        if (argc != 4) {
            std::cerr << "Error: encode mode requires exactly 2 arguments\n\n";
            print_usage();
            return 1;
        }

        std::string input_wav  = argv[2];
        std::string output_txt = argv[3];
        encode_audio(input_wav, output_txt);

    } else if (mode == "decode") {
        if (argc < 4 || argc > 6) {
            std::cerr << "Error: decode mode requires 2-4 arguments\n\n";
            print_usage();
            return 1;
        }

        std::string input_txt    = argv[2];
        std::string output_wav   = argv[3];
        uint32_t    sample_rate  = 44100;
        uint16_t    num_channels = 1;

        if (argc >= 5) {
            sample_rate = static_cast<uint32_t>(std::stoi(argv[4]));
        }

        if (argc >= 6) {
            num_channels = static_cast<uint16_t>(std::stoi(argv[5]));
        }

        decode_audio(input_txt, output_wav, sample_rate, num_channels);

    } else {
        std::cerr << "Error: Unknown mode '" << mode << "'\n\n";
        print_usage();
        return 1;
    }

    return 0;
}
