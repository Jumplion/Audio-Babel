// Simple CLI to extract an index from an audio file using AudioIndex
// Usage: extract_index_cli <input_wav> <out_index.bin>
#include <boost/multiprecision/cpp_int.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../include/AudioIndex.h"

using namespace std;
using boost::multiprecision::cpp_int;
using namespace AudioBabel;

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Usage: extract_index_cli <input_wav> <out_index.bin>" << endl;
        return 2;
    }
    string inPath  = argv[1];
    string outPath = argv[2];
    try {
        auto    audioData = AudioIndex::extractAudioDataFromAudioFile(inPath);
        cpp_int idx       = AudioIndex::audioDataToIndex(audioData);

        // For compatibility with the server's expectations, write the PCM payload
        // bytes (as stored in AudioData.samples) followed by the 16-byte header
        ofstream out(outPath, ios::binary);
        if (!out) {
            cerr << "Failed to open output file" << endl;
            return 3;
        }

        // Write raw PCM bytes as-is
        if (!audioData.samples.empty()) {
            out.write(reinterpret_cast<const char*>(audioData.samples.data()), audioData.samples.size());
        }

        // 16-byte big-endian header: sampleRate (u32), bitDepth (u16), numChannels (u16), numFrames (u64)
        auto write_be = [&](uint64_t val, size_t bytes) {
            for (int i = int(bytes) - 1; i >= 0; --i) {
                unsigned char c = static_cast<unsigned char>((val >> (8 * i)) & 0xFF);
                out.put(static_cast<char>(c));
            }
        };

        uint32_t sr     = static_cast<uint32_t>(audioData.sample_rate);
        uint16_t bd     = static_cast<uint16_t>(audioData.bit_rate);
        uint16_t ch     = static_cast<uint16_t>(audioData.num_channels);
        uint64_t frames = static_cast<uint64_t>(audioData.num_frames);

        write_be(sr, 4);
        write_be(bd, 2);
        write_be(ch, 2);
        write_be(frames, 8);

        out.close();
        return 0;
    } catch (const std::exception& ex) {
        cerr << "Exception: " << ex.what() << endl;
        return 1;
    }
}
