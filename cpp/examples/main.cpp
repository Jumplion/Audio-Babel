#include "AudioIndex.h"
#include "AudioFingerprint.h"
#include "AudioSearch.h"
#include "AudioBrowser.h"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace AudioBabel;

void demonstrateBrowsing() {
    std::cout << "=== Audio Babel Browsing Demo ===" << std::endl;
    
    AudioBrowser browser;
    
    // Browse genres
    std::cout << "\n--- Available Genres (Page 0) ---" << std::endl;
    auto genres = browser.listGenres(0, 5);
    for (size_t i = 0; i < genres.size(); ++i) {
        std::cout << i + 1 << ". " << genres[i] << std::endl;
    }
    
    // Browse artists in first genre
    if (!genres.empty()) {
        std::string selectedGenre = genres[0];
        std::cout << "\n--- Artists in Genre '" << selectedGenre << "' ---" << std::endl;
        auto artists = browser.listArtistsInGenre(selectedGenre, 0, 3);
        for (size_t i = 0; i < artists.size(); ++i) {
            std::cout << i + 1 << ". " << artists[i] << std::endl;
        }
        
        // Browse albums from first artist
        if (!artists.empty()) {
            std::string selectedArtist = artists[0];
            std::cout << "\n--- Albums by '" << selectedArtist << "' ---" << std::endl;
            auto albums = browser.listAlbumsFromArtist(selectedGenre, selectedArtist, 0, 3);
            for (size_t i = 0; i < albums.size(); ++i) {
                std::cout << i + 1 << ". " << albums[i] << std::endl;
            }
            
            // Browse tracks on first album
            if (!albums.empty()) {
                std::string selectedAlbum = albums[0];
                std::cout << "\n--- Tracks on '" << selectedAlbum << "' ---" << std::endl;
                auto tracks = browser.listTracksOnAlbum(selectedGenre, selectedArtist, selectedAlbum, 0, 3);
                for (size_t i = 0; i < tracks.size(); ++i) {
                    std::cout << i + 1 << ". " << tracks[i].getTrackString() << std::endl;
                }
            }
        }
    }
}

void demonstrateRandomGeneration() {
    std::cout << "\n=== Random Audio Generation Demo ===" << std::endl;
    
    AudioBrowser browser;
    
    // Generate random tracks
    std::cout << "\n--- Random Tracks ---" << std::endl;
    for (int i = 0; i < 3; ++i) {
        AudioIndex randomTrack = browser.getRandomTrack();
        std::cout << i + 1 << ". " << randomTrack.getFullPath() << std::endl;
        
        // Generate a small sample of audio
        std::vector<int32_t> samples = randomTrack.toAudioSamples();
        std::cout << "   Generated " << samples.size() << " audio samples" << std::endl;
    }
}

void demonstrateAudioProcessing() {
    std::cout << "\n=== Audio Processing Demo ===" << std::endl;
    
    // Create some synthetic audio data (sine wave)
    const int sampleRate = 48000;
    const int duration = 2; // 2 seconds
    const int numSamples = sampleRate * duration;
    const double frequency = 440.0; // A4 note
    
    std::vector<int32_t> samples(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double amplitude = 0.3 * std::sin(2.0 * M_PI * frequency * t);
        samples[i] = static_cast<int32_t>(amplitude * INT32_MAX);
    }
    
    std::cout << "Created synthetic sine wave (" << frequency << " Hz, " << duration << "s)" << std::endl;
    
    // Create audio index from samples
    AudioIndex audioIndex = AudioIndex::fromAudioSamples(samples, sampleRate);
    std::cout << "Generated audio index:" << std::endl;
    std::cout << "  Genre: " << audioIndex.getGenreString().substr(0, 20) << "..." << std::endl;
    std::cout << "  Artist: " << audioIndex.getArtistString().substr(0, 20) << "..." << std::endl;
    std::cout << "  Album: " << audioIndex.getAlbumString().substr(0, 20) << "..." << std::endl;
    std::cout << "  Track: " << audioIndex.getTrackString().substr(0, 20) << "..." << std::endl;
    
    // Test reconstruction
    std::vector<int32_t> reconstructed = audioIndex.toAudioSamples();
    std::cout << "Reconstructed " << reconstructed.size() << " samples" << std::endl;
    
    // Basic quality check (compare first few samples)
    if (!reconstructed.empty() && !samples.empty()) {
        double totalError = 0.0;
        int compareCount = std::min(100, static_cast<int>(std::min(samples.size(), reconstructed.size())));
        
        for (int i = 0; i < compareCount; ++i) {
            double error = std::abs(static_cast<double>(samples[i] - reconstructed[i])) / INT32_MAX;
            totalError += error;
        }
        
        double avgError = totalError / compareCount;
        std::cout << "Average reconstruction error (first " << compareCount << " samples): " 
                  << (avgError * 100.0) << "%" << std::endl;
    }
}

void demonstrateNavigation() {
    std::cout << "\n=== Navigation Demo ===" << std::endl;
    
    AudioBrowser browser;
    
    // Start with a random track
    AudioIndex currentTrack = browser.getRandomTrack();
    std::cout << "Starting track: " << currentTrack.getFullPath() << std::endl;
    
    // Navigate to next track
    AudioIndex nextTrack = browser.getNextTrack(currentTrack);
    std::cout << "Next track: " << nextTrack.getFullPath() << std::endl;
    
    // Navigate to previous track
    AudioIndex prevTrack = browser.getPreviousTrack(nextTrack);
    std::cout << "Previous track: " << prevTrack.getFullPath() << std::endl;
    
    // Verify we can navigate back to the original
    bool navigationWorks = (currentTrack == prevTrack);
    std::cout << "Navigation consistency: " << (navigationWorks ? "PASS" : "FAIL") << std::endl;
}

int main() {
    try {
        std::cout << "Audio Babel - Hierarchical Audio Indexing System" << std::endl;
        std::cout << "=================================================" << std::endl;
        
        demonstrateBrowsing();
        demonstrateRandomGeneration();
        demonstrateAudioProcessing();
        demonstrateNavigation();
        
        std::cout << "\n=== Demo Complete ===" << std::endl;
        std::cout << "The Audio Babel system successfully demonstrates:" << std::endl;
        std::cout << "• Hierarchical browsing (Genre → Artist → Album → Track)" << std::endl;
        std::cout << "• Random audio generation" << std::endl;
        std::cout << "• Audio-to-index conversion" << std::endl;
        std::cout << "• Index-to-audio reconstruction" << std::endl;
        std::cout << "• Sequential navigation through the audio space" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
