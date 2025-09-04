#include "AudioIndex.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <cstdint>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace AudioBabel;

int main() {
    auto usage = []() {
        std::cerr << "Audibel (AudioIndex) CLI Demo\n";
        std::cerr << "Usage:\n";
        std::cerr << "  audibelDemo.exe <input> [output]\n";
        std::cerr << "Description:\n";
        std::cerr << "  If <input> is a .wav file the program will read it and produce an index file as output.\n";
        std::cerr << "  If <input> is a decimal index file the program will reconstruct a .wav file as output.\n";
        std::cerr << "Examples:\n";
        std::cerr << "  audibelDemo.exe song.wav song_index.txt\n";
        std::cerr << "  audibelDemo.exe song_index.txt song_recon.wav\n";
    };

    if (__argc < 2) {
        usage();
        return 1;
    }

    std::string input = __argv[1];
    std::string output;
    if (__argc >= 3) output = __argv[2];

    auto extension = [](const std::string &p) {
        size_t pos = p.find_last_of('.');
        if (pos == std::string::npos) return std::string();
        std::string e = p.substr(pos);
        // lowercase
        for (auto &c : e) c = static_cast<char>(std::tolower(c));
        return e;
    };

    try {
        std::string ext = extension(input);
        
        // Audio File to Index
        if (ext == ".wav") {
            // WAV -> Index
            if (output.empty()) {
                // default output: input stem + ".txt"
                size_t pos = input.find_last_of(".");
                output = (pos == std::string::npos) ? (input + ".txt") : (input.substr(0, pos) + ".txt");
            }

            std::cerr << "Reading WAV: " << input << "\n";
            AudioIndex::AudioData data = AudioIndex::extractAudioDataFromAudioFile(input);

            std::cerr << "Sample rate: " << data.sample_rate << " Hz\n";
            std::cerr << "Bit depth : " << data.bit_rate << " bits\n";
            std::cerr << "Channels  : " << data.num_channels << "\n";
            std::cerr << "Frames    : " << data.num_frames << "\n";

            boost::multiprecision::cpp_int idx = AudioIndex::audioDataToIndex(data);

            // write index representation
            std::ofstream out(output, std::ios::out | std::ios::trunc);
            if (!out) throw std::runtime_error("Failed to open output index file: " + output);
            //out << idx.convert_to<std::string>() << "\n";
            out.close();

            std::cerr << "Wrote index to: " << output << "\n";
            
            // also write auxiliary representations using the chosen output prefix (no extension)
            try {
                std::filesystem::path outp(output);
                // prefer to write auxiliary files in the same directory as the output
                std::string dir = outp.has_parent_path() ? outp.parent_path().string() : std::string();
                std::string stem = outp.replace_extension().filename().string();
                AudioIndex::writeIndexRepresentations(idx, dir, stem);
                std::cerr << "Also wrote other representations into directory: " << (dir.empty() ? "cpp/tests/indexes" : dir) << " with stem " << stem << "\n";
            } catch (...) {
                // best-effort only
            }

            return 0;
        } 
        
        // Index to Audio File
        else {
            // Assume index input -> WAV
            if (output.empty()) {
                size_t pos = input.find_last_of(".");
                output = (pos == std::string::npos) ? (input + ".wav") : (input.substr(0, pos) + ".wav");
            }

            // Read file into string and parse decimal
            std::ifstream in(input);
            if (!in) {
                std::cerr << "Failed to open input file: " << input << "\n";
                usage();
                return 2;
            }
            std::string contents;
            std::string line;
            while (std::getline(in, line)) {
                if (!line.empty()) {
                    contents += line;
                }
            }
            in.close();

            if (contents.empty()) {
                throw std::runtime_error("Empty index file: " + input);
            }

            boost::multiprecision::cpp_int idx;
            {
                std::istringstream ss(contents);
                ss >> idx;
                if (ss.fail()) throw std::runtime_error("Failed to parse index from file: " + input);
            }

            std::cerr << "Reconstructing WAV from index...\n";
            AudioIndex::AudioData data = AudioIndex::indexToAudioData(idx);
            AudioIndex::writeAudioDataToFile(data, output);
            std::cerr << "Wrote WAV to: " << output << "\n";
            return 0;
        }
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 3;
    }
}
