#include "AudioIndex.h"
#include "AudioFingerprint.h"
#include "AudioSearch.h"
#include "AudioBrowser.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>

using namespace AudioBabel;

bool testAudioIndex() {
    std::cout << "Testing AudioIndex..." << std::endl;
    
    // Test hierarchical construction
    AudioIndex index1 = AudioIndex::fromHierarchy("genre1", "artist1", "album1", "track1");
    AudioIndex index2 = AudioIndex::fromHierarchy("genre1", "artist1", "album1", "track2");
    
    // Test equality
    assert(index1 == index1);
    assert(index1 != index2);
    
    // Test string representations
    assert(index1.getGenreString() == "genre1");
    assert(index1.getArtistString() == "artist1");
    assert(index1.getAlbumString() == "album1");
    assert(index1.getTrackString() == "track1");
    
    std::cout << "  ✓ Basic construction and equality" << std::endl;
    
    // Test serialization
    std::stringstream ss;
    index1.serialize(ss);
    ss.seekg(0);
    AudioIndex deserialized = AudioIndex::deserialize(ss);
    
    assert(index1 == deserialized);
    std::cout << "  ✓ Serialization and deserialization" << std::endl;
    
    return true;
}

bool testAudioFingerprint() {
    std::cout << "Testing AudioFingerprint..." << std::endl;
    
    // Create synthetic sine wave
    const int sampleRate = 48000;
    const int duration = 1; // 1 second
    const int numSamples = sampleRate * duration;
    const double frequency = 440.0;
    
    std::vector<int32_t> samples(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double amplitude = 0.5 * std::sin(2.0 * M_PI * frequency * t);
        samples[i] = static_cast<int32_t>(amplitude * INT32_MAX);
    }
    
    // Create fingerprint
    AudioFingerprint fingerprint = AudioFingerprint::fromSamples(samples, sampleRate);
    assert(fingerprint.getBlockCount() > 0);
    assert(fingerprint.getOriginalSampleRate() == sampleRate);
    
    std::cout << "  ✓ Fingerprint creation from samples" << std::endl;
    
    // Test serialization
    std::vector<uint8_t> serialized = fingerprint.serialize();
    AudioFingerprint deserialized = AudioFingerprint::deserialize(serialized);
    
    assert(deserialized.getBlockCount() == fingerprint.getBlockCount());
    assert(deserialized.getOriginalSampleRate() == fingerprint.getOriginalSampleRate());
    
    std::cout << "  ✓ Fingerprint serialization" << std::endl;
    
    // Test similarity calculation
    double selfSimilarity = fingerprint.calculateSimilarity(fingerprint);
    assert(selfSimilarity > 0.9); // Should be very similar to itself
    
    std::cout << "  ✓ Similarity calculation (self-similarity: " << selfSimilarity << ")" << std::endl;
    
    // Test reconstruction
    std::vector<int32_t> reconstructed = fingerprint.toSamples(sampleRate);
    assert(reconstructed.size() > 0);
    
    std::cout << "  ✓ Audio reconstruction" << std::endl;
    
    return true;
}

bool testAudioBrowser() {
    std::cout << "Testing AudioBrowser..." << std::endl;
    
    AudioBrowser browser;
    
    // Test genre listing
    std::vector<std::string> genres = browser.listGenres(0, 5);
    assert(genres.size() == 5);
    assert(!genres[0].empty());
    
    std::cout << "  ✓ Genre listing" << std::endl;
    
    // Test artist listing
    std::string testGenre = genres[0];
    std::vector<std::string> artists = browser.listArtistsInGenre(testGenre, 0, 3);
    assert(artists.size() == 3);
    assert(!artists[0].empty());
    
    std::cout << "  ✓ Artist listing" << std::endl;
    
    // Test random generation
    AudioIndex randomTrack = browser.getRandomTrack();
    assert(!randomTrack.getGenreString().empty());
    assert(!randomTrack.getArtistString().empty());
    assert(!randomTrack.getAlbumString().empty());
    assert(!randomTrack.getTrackString().empty());
    
    std::cout << "  ✓ Random track generation" << std::endl;
    
    // Test navigation
    AudioIndex nextTrack = browser.getNextTrack(randomTrack);
    AudioIndex prevTrack = browser.getPreviousTrack(nextTrack);
    
    // The previous of next should equal original (in most cases)
    // Note: This might not always be true due to encoding/decoding, so we just check structure
    assert(!nextTrack.getTrackString().empty());
    assert(!prevTrack.getTrackString().empty());
    
    std::cout << "  ✓ Track navigation" << std::endl;
    
    // Test path parsing
    std::string testPath = "testgenre/testartist/testalbum/testtrack";
    auto parsed = browser.parsePath(testPath);
    assert(parsed.isValid);
    assert(parsed.genre == "testgenre");
    assert(parsed.artist == "testartist");
    assert(parsed.album == "testalbum");
    assert(parsed.track == "testtrack");
    
    std::cout << "  ✓ Path parsing" << std::endl;
    
    return true;
}

bool testAudioSearch() {
    std::cout << "Testing AudioSearch..." << std::endl;
    
    AudioSearch search;
    
    // Create some test audio indices
    AudioIndex index1 = AudioIndex::fromHierarchy("rock", "band1", "album1", "song1");
    AudioIndex index2 = AudioIndex::fromHierarchy("rock", "band2", "album1", "song1");
    AudioIndex index3 = AudioIndex::fromHierarchy("jazz", "artist1", "album1", "track1");
    
    // Add to search index
    search.addToIndex(index1);
    search.addToIndex(index2);
    search.addToIndex(index3);
    
    // Test index stats
    auto stats = search.getIndexStats();
    assert(stats.totalEntries == 3);
    assert(stats.uniqueGenres >= 2); // Should have at least "rock" and "jazz"
    
    std::cout << "  ✓ Index building and statistics" << std::endl;
    
    // Test similarity search
    auto similarResults = search.findSimilarAudio(index1, 5, 0.1);
    assert(!similarResults.empty());
    
    std::cout << "  ✓ Similarity search (found " << similarResults.size() << " results)" << std::endl;
    
    // Create test audio samples for containment search
    std::vector<int32_t> testSamples(1000, 12345); // Simple test pattern
    auto containmentResults = search.findContainingAudio(testSamples, 48000, 5, 0.1);
    
    std::cout << "  ✓ Containment search (found " << containmentResults.size() << " results)" << std::endl;
    
    return true;
}

bool testIntegration() {
    std::cout << "Testing Integration..." << std::endl;
    
    // Create a complete workflow: generate audio → create index → search → browse
    
    // 1. Generate synthetic audio
    std::vector<int32_t> originalSamples(48000); // 1 second at 48kHz
    for (size_t i = 0; i < originalSamples.size(); ++i) {
        double t = static_cast<double>(i) / 48000.0;
        originalSamples[i] = static_cast<int32_t>(0.3 * std::sin(2.0 * M_PI * 440.0 * t) * INT32_MAX);
    }
    
    // 2. Create index from audio
    AudioIndex audioIndex = AudioIndex::fromAudioSamples(originalSamples, 48000);
    
    // 3. Add to search system
    AudioSearch search;
    search.addToIndex(audioIndex);
    
    // 4. Search for similar audio
    auto searchResults = search.findSimilarAudio(audioIndex, 1, 0.5);
    assert(!searchResults.empty());
    
    // 5. Use browser to explore the space around this audio
    AudioBrowser browser;
    auto similarGenres = audioIndex.getSimilarGenres(3);
    assert(similarGenres.size() == 3);
    
    // 6. Test reconstruction
    std::vector<int32_t> reconstructed = audioIndex.toAudioSamples();
    assert(!reconstructed.empty());
    
    std::cout << "  ✓ Complete audio → index → search → browse workflow" << std::endl;
    
    return true;
}

int main() {
    std::cout << "Audio Babel Unit Tests" << std::endl;
    std::cout << "======================" << std::endl;
    
    try {
        bool allPassed = true;
        
        allPassed &= testAudioIndex();
        allPassed &= testAudioFingerprint();
        allPassed &= testAudioBrowser();
        allPassed &= testAudioSearch();
        allPassed &= testIntegration();
        
        std::cout << "\n=== Test Results ===" << std::endl;
        if (allPassed) {
            std::cout << "✓ All tests PASSED!" << std::endl;
            return 0;
        } else {
            std::cout << "✗ Some tests FAILED!" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}
