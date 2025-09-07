#include "AudioIndex.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace AudioBabel;

// Simple CLI:
// Usage: reconstruct_cli <input.bin> <output.wav>
// Reads raw index bytes from input.bin, imports them into a cpp_int, then
// calls indexToAudioData and writes WAV to output.wav.

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: reconstruct_cli <index.bin> <output.wav>\n";
        return 2;
    }
    const std::string inPath = argv[1];
    const std::string outPath = argv[2];

    std::ifstream in(inPath, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open input file: " << inPath << "\n";
        return 3;
    }
    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (buf.empty()) {
        std::cerr << "Input file empty: " << inPath << "\n";
        return 4;
    }

    // Import bytes as big-endian into cpp_int
    boost::multiprecision::cpp_int idx = 0;
    boost::multiprecision::import_bits(idx, buf.begin(), buf.end(), 8, true);

    try {
        AudioIndex::AudioData ad = AudioIndex::indexToAudioData(idx);
        AudioIndex::exportAudioDataToWav(ad, outPath);
    } catch (const std::exception& e) {
        std::cerr << "Reconstruction failed: " << e.what() << "\n";
        return 5;
    }
    return 0;
}
