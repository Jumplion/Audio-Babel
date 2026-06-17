#include <cctype>
#include <fstream>
#include <iostream>
#include <string>

#include "AudioIndex.h"
#include "FileWriters.h"
#include "Utilities.h"

using namespace AudioBabel;

// Simple CLI:
// Usage: reconstruct_cli <index.txt> <output.wav>
// Reads a bijective URL-safe base-64 index string from index.txt, converts it
// to a cpp_int via b64ToIndex, then calls indexToAudioData and writes a WAV
// (default header: PCM, 44100 Hz, 16-bit, mono) to output.wav.

auto main(int argc, char** argv) -> int {
    if (argc != 3) {
        std::cerr << "Usage: reconstruct_cli <index.txt> <output.wav>\n";
        return 2;
    }
    const std::string inPath  = argv[1];
    const std::string outPath = argv[2];

    std::ifstream in(inPath);
    if (!in) {
        std::cerr << "Failed to open input file: " << inPath << "\n";
        return 3;
    }

    // Read the index string, ignoring any surrounding whitespace/newlines.
    std::string indexStr;
    {
        std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        for (char c : raw) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                indexStr.push_back(c);
            }
        }
    }

    try {
        boost::multiprecision::cpp_int idx = Utilities::b64ToIndex(indexStr);
        AudioIndex::AudioData          ad  = AudioIndex::indexToAudioData(idx);
        FileWriters::exportAudioDataToWav(ad, outPath);
    } catch (const std::exception& e) {
        std::cerr << "Reconstruction failed: " << e.what() << "\n";
        return 5;
    }
    return 0;
}
