#include "AudioIndex.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdint>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace AudioBabel;

// Focused demo: only show indexing and reconstruction using AudioIndex
void demonstrateIndexing() {
    std::cout << "=== AudioIndexing Demo ===" << std::endl;

    // Create synthetic sine wave samples
    const int sampleRate = 44100;
    const int duration = 1; // 1 second for faster demo
    const int numSamples = sampleRate * duration;
    const double frequency = 440.0; // A4

    std::vector<int32_t> samples(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double amplitude = 0.3 * std::sin(2.0 * M_PI * frequency * t);
        samples[i] = static_cast<int32_t>(amplitude * INT32_MAX);
    }

    std::cout << "Created " << numSamples << " synthetic samples (" << frequency << " Hz)" << std::endl;

    // Build an AudioIndex from samples
    AudioIndex idx = AudioIndex::fromAudioSamples(samples, sampleRate);
    std::cout << "Index built from audio samples:" << std::endl;
    std::cout << "  Genre:  " << idx.getGenreString() << std::endl;
    std::cout << "  Artist: " << idx.getArtistString() << std::endl;
    std::cout << "  Album:  " << idx.getAlbumString() << std::endl;
    std::cout << "  Track:  " << idx.getTrackString() << std::endl;

    // Attempt reconstruction (may be a no-op depending on implementation)
    std::vector<int32_t> recon = idx.toAudioSamples();
    std::cout << "Reconstructed samples: " << recon.size() << std::endl;

    // Also demonstrate deterministic indexing from explicit hierarchy
    AudioIndex fromHierarchy = AudioIndex::fromHierarchy("Rock", "DemoArtist", "DemoAlbum", "DemoTrack");
    std::cout << "Index from hierarchy (Rock/DemoArtist/DemoAlbum/DemoTrack):" << std::endl;
    std::cout << "  Fingerprint size: " << fromHierarchy.getFingerprint().size() << " bytes" << std::endl;
}

int main() {
    try {
        std::cout << "Audio Babel - focused indexing demo" << std::endl;
        std::cout << "================================" << std::endl;
        demonstrateIndexing();
        std::cout << "\nDemo finished." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
