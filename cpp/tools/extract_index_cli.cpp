// Simple CLI to extract an index from an audio file using AudioIndex
// Usage: extract_index_cli <input_wav> <out_index.txt>
//
// Writes the canonical payload-only index as a bijective URL-safe base-64
// string (see docs/INDEX_FORMAT.md). The previous raw-PCM-plus-16-byte-trailer
// format has been removed in favour of this single canonical encoding.
#include <boost/multiprecision/cpp_int.hpp>
#include <fstream>
#include <iostream>
#include <string>

#include "../include/AudioIndex.h"
#include "../include/Utilities.h"

using namespace std;
using boost::multiprecision::cpp_int;
using namespace AudioBabel;

auto main(int argc, char** argv) -> int {
    if (argc < 3) {
        cerr << "Usage: extract_index_cli <input_wav> <out_index.txt>" << '\n';
        return 2;
    }
    string inPath  = argv[1];
    string outPath = argv[2];
    try {
        auto    audioData = AudioIndex::extractAudioDataFromAudioFile(inPath);
        cpp_int idx       = AudioIndex::audioDataToIndex(audioData);

        ofstream out(outPath);
        if (!out) {
            cerr << "Failed to open output file" << '\n';
            return 3;
        }

        out << Utilities::indexToB64(idx) << '\n';
        out.close();
        return 0;
    } catch (const std::exception& ex) {
        cerr << "Exception: " << ex.what() << '\n';
        return 1;
    }
}
