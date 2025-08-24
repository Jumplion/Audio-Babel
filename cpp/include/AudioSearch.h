#ifndef AUDIO_SEARCH_H
#define AUDIO_SEARCH_H

/*
 * AudioSearch.h
 * --------------
 * Purpose: Public API for building and querying a search index of AudioIndex
 *          objects. Supports containment and perceptual similarity searches.
 *
 * Contract highlights:
 *  - addToIndex accepts an AudioIndex and (optionally) a filePath for lazy loading.
 *  - Searches operate on AudioFingerprint objects; callers should expect async
 *    internal processing and eventual vector results ordered by similarity.
 */

#include "AudioFingerprint.h"
#include "AudioIndex.h"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>

namespace AudioBabel {

/**
 * Search system for finding audio clips that contain or are similar to query audio
 * Supports both exact containment search and perceptual similarity search
 */
class AudioSearch {
private:
    struct IndexEntry {
        std::unique_ptr<AudioIndex> index;
        std::unique_ptr<AudioFingerprint> fingerprint;
        std::string filePath;  // For lazy loading
        
        IndexEntry() = default;
        IndexEntry(IndexEntry&& other) noexcept 
            : index(std::move(other.index))
            , fingerprint(std::move(other.fingerprint))
            , filePath(std::move(other.filePath)) {}
        
        IndexEntry& operator=(IndexEntry&& other) noexcept {
            if (this != &other) {
                index = std::move(other.index);
                fingerprint = std::move(other.fingerprint);
                filePath = std::move(other.filePath);
            }
            return *this;
        }
    };
    
    // Search index storage
    std::vector<IndexEntry> searchIndex;
    
    // Hash tables for faster lookups
    std::unordered_map<std::string, std::vector<size_t>> genreIndex;
    std::unordered_map<std::string, std::vector<size_t>> artistIndex;
    
    // Thread safety
    mutable std::mutex indexMutex;
    
    // Configuration
    int maxConcurrentSearches;
    double defaultSimilarityThreshold;
    
public:
    AudioSearch();
    ~AudioSearch();
    
    // Index management
    /**
     * Build search index from a directory of audio index files
     * @param indexDirectory Directory containing serialized AudioIndex files
     * @param recursive Whether to search subdirectories
     */
    void buildSearchIndex(const std::string& indexDirectory, bool recursive = true);
    
    /**
     * Add a single audio index to the search system
     * @param audioIndex AudioIndex to add
     * @param filePath Optional file path for lazy loading
     */
    void addToIndex(const AudioIndex& audioIndex, const std::string& filePath = "");
    
    /**
     * Clear the entire search index
     */
    void clearIndex();
    
    /**
     * Get statistics about the search index
     */
    struct IndexStats {
        size_t totalEntries;
        size_t uniqueGenres;
        size_t uniqueArtists;
        size_t memoryUsage;  // Approximate memory usage in bytes
    };
    IndexStats getIndexStats() const;
    
    // Search operations
    /**
     * Find audio indexes that contain the query audio
     * @param querySamples PCM audio samples to search for
     * @param sampleRate Sample rate of query audio
     * @param maxResults Maximum number of results to return
     * @param minSimilarity Minimum similarity threshold (0.0-1.0)
     * @return Vector of matching AudioIndex objects with similarity scores
     */
    struct SearchResult {
        AudioIndex index;
        double similarityScore;
        double containmentScore;  // How well the query fits within this result
        int offsetMs;  // Offset in milliseconds where match was found
    };
    
    std::vector<SearchResult> findContainingAudio(const std::vector<int32_t>& querySamples, 
                                                 int sampleRate,
                                                 int maxResults = 10,
                                                 double minSimilarity = 0.7);
    
    /**
     * Find audio indexes with similar perceptual characteristics
     * @param reference Reference AudioIndex for comparison
     * @param maxResults Maximum number of results to return
     * @param minSimilarity Minimum similarity threshold (0.0-1.0)
     * @return Vector of similar AudioIndex objects with similarity scores
     */
    std::vector<SearchResult> findSimilarAudio(const AudioIndex& reference,
                                              int maxResults = 10,
                                              double minSimilarity = 0.7);
    
    /**
     * Search within a specific genre or artist
     * @param genreFilter Genre to filter by (empty for no filter)
     * @param artistFilter Artist to filter by (empty for no filter)
     * @param querySamples PCM audio samples to search for
     * @param sampleRate Sample rate of query audio
     * @param maxResults Maximum number of results
     * @param minSimilarity Minimum similarity threshold
     */
    std::vector<SearchResult> findWithinCategory(const std::string& genreFilter,
                                                const std::string& artistFilter,
                                                const std::vector<int32_t>& querySamples,
                                                int sampleRate,
                                                int maxResults = 10,
                                                double minSimilarity = 0.7);
    
    // Advanced search features
    /**
     * Find audio that sounds similar but has different hierarchical codes
     * Useful for discovering "cover versions" or similar compositions
     */
    std::vector<SearchResult> findPerceptualDuplicates(double similarityThreshold = 0.9);
    
    /**
     * Search for audio with specific spectral characteristics
     * @param targetCentroid Target spectral centroid
     * @param targetRolloff Target spectral rolloff
     * @param tolerance Tolerance for matching (0.0-1.0)
     */
    std::vector<SearchResult> findBySpectralFeatures(double targetCentroid,
                                                    double targetRolloff,
                                                    double tolerance = 0.1,
                                                    int maxResults = 10);
    
    // Configuration
    void setMaxConcurrentSearches(int maxSearches) { maxConcurrentSearches = maxSearches; }
    void setDefaultSimilarityThreshold(double threshold) { defaultSimilarityThreshold = threshold; }
    
    // Persistence
    void saveIndex(const std::string& indexFilePath) const;
    void loadIndex(const std::string& indexFilePath);
    
private:
    // Internal search helpers
    std::vector<SearchResult> performParallelSearch(
        const AudioFingerprint& queryFingerprint,
        const std::vector<size_t>& candidateIndices,
        int maxResults,
        double minSimilarity) const;
    
    std::vector<size_t> getCandidates(const std::string& genreFilter,
                                    const std::string& artistFilter) const;
    
    // Lazy loading
    void ensureLoaded(IndexEntry& entry) const;
    
    // Index building helpers
    void processIndexFile(const std::string& filePath);
    void updateHashTables(size_t entryIndex);
};

} // namespace AudioBabel

/* Known issues / suggested fixes:
 *  - `ensureLoaded` mutates IndexEntry without strong synchronization. If entries
 *    are accessed concurrently, lazy loading can race with other reads. Consider
 *    pre-loading or using double-checked locking with atomics/locks.
 *  - `updateHashTables` sorts vectors on every insertion; this is inefficient.
 *    Maintain invariants during build or sort once after bulk insertion.
 *  - performParallelSearch captures `this` and accesses `searchIndex` concurrently;
 *    ensure proper synchronization or snapshot the list of entries before parallelism.
 */

#endif // AUDIO_SEARCH_H
