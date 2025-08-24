#include "AudioSearch.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <execution>
#include <future>
#include <iostream>

namespace AudioBabel {

/*
 * AudioSearch.cpp
 * ----------------
 * Purpose: Implements building, persistence, and parallel search over the
 *          audio index. Uses std::async to parallelize candidate matching.
 *
 * Concurrency notes:
 *  - performParallelSearch captures `this` and reads `searchIndex` concurrently;
 *    ensure any mutation paths are synchronized or snapshot the index before search.
 *  - ensureLoaded performs IO while holding no external lock; callers must ensure
 *    safe access patterns when using lazy loading across threads.
 */
AudioSearch::AudioSearch() 
    : maxConcurrentSearches(std::thread::hardware_concurrency()),
        defaultSimilarityThreshold(0.7) {
}

AudioSearch::~AudioSearch() {
}

void AudioSearch::buildSearchIndex(const std::string& indexDirectory, bool recursive) {
    std::lock_guard<std::mutex> lock(indexMutex);

    clearIndex();

    std::filesystem::path dirPath(indexDirectory);
    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
        throw std::runtime_error("Index directory does not exist: " + indexDirectory);
    }

    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
                if (!entry.is_regular_file()) continue;
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(),
                            [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

                if (extension == ".aidx" || extension == ".audioidx") {
                    try {
                        processIndexFile(entry.path().string());
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to process index file " << entry.path()
                                << ": " << e.what() << std::endl;
                    }
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
                if (!entry.is_regular_file()) continue;
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(),
                            [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

                if (extension == ".aidx" || extension == ".audioidx") {
                    try {
                        processIndexFile(entry.path().string());
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to process index file " << entry.path()
                                << ": " << e.what() << std::endl;
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& fe) {
        std::cerr << "Filesystem error while building search index: " << fe.what() << std::endl;
        // decide whether to rethrow or continue; currently just logs
    }

    std::cout << "Built search index with " << searchIndex.size() << " entries" << std::endl;
}

void AudioSearch::addToIndex(const AudioIndex& audioIndex, const std::string& filePath) {
    std::lock_guard<std::mutex> lock(indexMutex);
    
    IndexEntry entry;
    entry.index = std::make_unique<AudioIndex>(audioIndex);
    entry.fingerprint = std::make_unique<AudioFingerprint>(
        AudioFingerprint::deserialize(audioIndex.getFingerprint())
    );
    entry.filePath = filePath;
    
    size_t entryIndex = searchIndex.size();
    searchIndex.emplace_back(std::move(entry));
    updateHashTables(entryIndex);
}

void AudioSearch::clearIndex() {
    searchIndex.clear();
    genreIndex.clear();
    artistIndex.clear();
}

AudioSearch::IndexStats AudioSearch::getIndexStats() const {
    std::lock_guard<std::mutex> lock(indexMutex);
    
    IndexStats stats;
    stats.totalEntries = searchIndex.size();
    stats.uniqueGenres = genreIndex.size();
    stats.uniqueArtists = artistIndex.size();
    
    // Rough memory usage estimate
    stats.memoryUsage = searchIndex.size() * (
        sizeof(IndexEntry) + 
        (1024 * 1024) + // Rough estimate for fingerprint + index
        256  // Hash table overhead
    );
    
    return stats;
}

std::vector<AudioSearch::SearchResult> AudioSearch::findContainingAudio(const std::vector<int32_t>& querySamples, int sampleRate, int maxResults, double minSimilarity) {
    
    if (querySamples.empty()) {
        return {};
    }
    
    // Create fingerprint from query
    AudioFingerprint queryFingerprint = AudioFingerprint::fromSamples(querySamples, sampleRate);
    
    // Get all candidates (for now, search all entries)
    std::vector<size_t> candidates;
    candidates.reserve(searchIndex.size());
    for (size_t i = 0; i < searchIndex.size(); ++i) {
        candidates.push_back(i);
    }
    
    return performParallelSearch(queryFingerprint, candidates, maxResults, minSimilarity);
}

std::vector<AudioSearch::SearchResult> AudioSearch::findSimilarAudio(const AudioIndex& reference, int maxResults, double minSimilarity) {
    
    if (reference.getFingerprint().empty()) {
        return {};
    }
    
    AudioFingerprint referenceFingerprint = AudioFingerprint::deserialize(reference.getFingerprint());
    
    // Filter candidates by genre for more efficient search
    std::vector<size_t> candidates = getCandidates(reference.getGenreString(), "");
    
    return performParallelSearch(referenceFingerprint, candidates, maxResults, minSimilarity);
}

std::vector<AudioSearch::SearchResult> AudioSearch::findWithinCategory(
    const std::string& genreFilter,
    const std::string& artistFilter,
    const std::vector<int32_t>& querySamples,
    int sampleRate,
    int maxResults,
    double minSimilarity) {
    
    if (querySamples.empty()) {
        return {};
    }
    
    AudioFingerprint queryFingerprint = AudioFingerprint::fromSamples(querySamples, sampleRate);
    std::vector<size_t> candidates = getCandidates(genreFilter, artistFilter);
    
    return performParallelSearch(queryFingerprint, candidates, maxResults, minSimilarity);
}

std::vector<AudioSearch::SearchResult> AudioSearch::findPerceptualDuplicates(double similarityThreshold) {
    std::vector<SearchResult> duplicates;
    
    std::lock_guard<std::mutex> lock(indexMutex);
    
    for (size_t i = 0; i < searchIndex.size(); ++i) {
        ensureLoaded(searchIndex[i]);
        
        for (size_t j = i + 1; j < searchIndex.size(); ++j) {
            ensureLoaded(searchIndex[j]);
            
            double similarity = searchIndex[i].fingerprint->calculateSimilarity(
                *searchIndex[j].fingerprint
            );
            
            if (similarity >= similarityThreshold) {
                // Check if they have different hierarchical codes
                if (*searchIndex[i].index != *searchIndex[j].index) {
                    SearchResult result;
                    result.index = *searchIndex[j].index;
                    result.similarityScore = similarity;
                    result.containmentScore = similarity;
                    result.offsetMs = 0;
                    duplicates.push_back(result);
                }
            }
        }
    }
    
    return duplicates;
}

std::vector<AudioSearch::SearchResult> AudioSearch::findBySpectralFeatures(
    double targetCentroid,
    double targetRolloff,
    double tolerance,
    int maxResults) {
    
    std::vector<SearchResult> results;
    
    std::lock_guard<std::mutex> lock(indexMutex);
    
    for (size_t i = 0; i < searchIndex.size(); ++i) {
        ensureLoaded(searchIndex[i]);
        
        // Calculate spectral features
        std::vector<double> centroid = searchIndex[i].fingerprint->getSpectralCentroid();
        std::vector<double> rolloff = searchIndex[i].fingerprint->getSpectralRolloff();
        
        if (!centroid.empty() && !rolloff.empty()) {
            double avgCentroid = std::accumulate(centroid.begin(), centroid.end(), 0.0) / centroid.size();
            double avgRolloff = std::accumulate(rolloff.begin(), rolloff.end(), 0.0) / rolloff.size();
            
            double centroidDiff = std::abs(avgCentroid - targetCentroid) / targetCentroid;
            double rolloffDiff = std::abs(avgRolloff - targetRolloff) / targetRolloff;
            
            if (centroidDiff <= tolerance && rolloffDiff <= tolerance) {
                SearchResult result;
                result.index = *searchIndex[i].index;
                result.similarityScore = 1.0 - (centroidDiff + rolloffDiff) / 2.0;
                result.containmentScore = result.similarityScore;
                result.offsetMs = 0;
                results.push_back(result);
            }
        }
    }
    
    // Sort by similarity and limit results
    std::sort(results.begin(), results.end(), 
            [](const SearchResult& a, const SearchResult& b) {
                return a.similarityScore > b.similarityScore;
            });
    
    if (results.size() > static_cast<size_t>(maxResults)) {
        results.resize(maxResults);
    }
    
    return results;
}

void AudioSearch::saveIndex(const std::string& indexFilePath) const {
    std::lock_guard<std::mutex> lock(indexMutex);
    
    std::ofstream file(indexFilePath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open index file for writing: " + indexFilePath);
    }
    
    // Write number of entries
    uint64_t numEntries = searchIndex.size();
    file.write(reinterpret_cast<const char*>(&numEntries), sizeof(numEntries));
    
    // Write each entry
    for (const auto& entry : searchIndex) {
        if (entry.index && entry.fingerprint) {
            entry.index->serialize(file);
        }
    }
}

void AudioSearch::loadIndex(const std::string& indexFilePath) {
    std::lock_guard<std::mutex> lock(indexMutex);
    
    clearIndex();
    
    std::ifstream file(indexFilePath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open index file for reading: " + indexFilePath);
    }
    
    // Read number of entries
    uint64_t numEntries;
    file.read(reinterpret_cast<char*>(&numEntries), sizeof(numEntries));
    
    searchIndex.reserve(numEntries);
    
    // Read each entry
    for (uint64_t i = 0; i < numEntries; ++i) {
        try {
            AudioIndex index = AudioIndex::deserialize(file);
            addToIndex(index);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load index entry " << i << ": " << e.what() << std::endl;
        }
    }
}

// Private helper methods
std::vector<AudioSearch::SearchResult> AudioSearch::performParallelSearch(
    const AudioFingerprint& queryFingerprint,
    const std::vector<size_t>& candidateIndices,
    int maxResults,
    double minSimilarity) const {
    
    std::vector<SearchResult> results;
    std::mutex resultsMutex;
    
    // Determine chunk size for parallel processing
    size_t chunkSize = std::max(size_t(1), candidateIndices.size() / maxConcurrentSearches);
    std::vector<std::future<std::vector<SearchResult>>> futures;
    
    for (size_t start = 0; start < candidateIndices.size(); start += chunkSize) {
        size_t end = std::min(start + chunkSize, candidateIndices.size());
        
        auto future = std::async(std::launch::async, [this, &queryFingerprint, &candidateIndices, start, end, minSimilarity]() {
            std::vector<SearchResult> chunkResults;
            
            for (size_t i = start; i < end; ++i) {
                size_t candidateIndex = candidateIndices[i];
                if (candidateIndex >= searchIndex.size()) continue;
                
                const auto& entry = searchIndex[candidateIndex];
                ensureLoaded(const_cast<IndexEntry&>(entry));
                
                if (!entry.fingerprint) continue;
                
                double similarity;
                bool contains = entry.fingerprint->contains(queryFingerprint, similarity);
                
                if (similarity >= minSimilarity) {
                    SearchResult result;
                    result.index = *entry.index;
                    result.similarityScore = similarity;
                    result.containmentScore = contains ? similarity : 0.0;
                    result.offsetMs = 0; // TODO: Calculate actual offset
                    chunkResults.push_back(result);
                }
            }
            
            return chunkResults;
        });
        
        futures.push_back(std::move(future));
    }
    
    // Collect results from all threads
    for (auto& future : futures) {
        try {
            auto chunkResults = future.get();
            results.insert(results.end(), chunkResults.begin(), chunkResults.end());
        } catch (const std::exception& e) {
            std::cerr << "Search thread failed: " << e.what() << std::endl;
        }
    }
    
    // Sort by similarity score and limit results
    std::sort(results.begin(), results.end(), 
            [](const SearchResult& a, const SearchResult& b) {
                return a.similarityScore > b.similarityScore;
            });
    
    if (results.size() > static_cast<size_t>(maxResults)) {
        results.resize(maxResults);
    }
    
    return results;
}

std::vector<size_t> AudioSearch::getCandidates(const std::string& genreFilter, const std::string& artistFilter) const {
    std::vector<size_t> candidates;
    
    if (!genreFilter.empty()) {
        auto it = genreIndex.find(genreFilter);
        if (it != genreIndex.end()) {
            candidates = it->second;
            
            if (!artistFilter.empty()) {
                auto artistIt = artistIndex.find(artistFilter);
                if (artistIt != artistIndex.end()) {
                    // Intersect genre and artist candidates
                    std::vector<size_t> intersection;
                    std::set_intersection(
                        candidates.begin(), candidates.end(),
                        artistIt->second.begin(), artistIt->second.end(),
                        std::back_inserter(intersection)
                    );
                    candidates = intersection;
                }
            }
        }
    } else if (!artistFilter.empty()) {
        auto it = artistIndex.find(artistFilter);
        if (it != artistIndex.end()) {
            candidates = it->second;
        }
    } else {
        // No filters, return all indices
        candidates.reserve(searchIndex.size());
        for (size_t i = 0; i < searchIndex.size(); ++i) {
            candidates.push_back(i);
        }
    }
    
    return candidates;
}

void AudioSearch::ensureLoaded(IndexEntry& entry) const {
    if (!entry.index && !entry.filePath.empty()) {
        try {
            std::ifstream file(entry.filePath, std::ios::binary);
            if (file.is_open()) {
                entry.index = std::make_unique<AudioIndex>(AudioIndex::deserialize(file));
                entry.fingerprint = std::make_unique<AudioFingerprint>(
                    AudioFingerprint::deserialize(entry.index->getFingerprint())
                );
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to lazy load index from " << entry.filePath 
                    << ": " << e.what() << std::endl;
        }
    }
}

void AudioSearch::processIndexFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open index file: " + filePath);
    }
    
    try {
        AudioIndex index = AudioIndex::deserialize(file);
        addToIndex(index, filePath);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to process index file " + filePath + ": " + e.what());
    }
}

void AudioSearch::updateHashTables(size_t entryIndex) {
    const auto& entry = searchIndex[entryIndex];
    if (entry.index) {
        genreIndex[entry.index->getGenreString()].push_back(entryIndex);
        artistIndex[entry.index->getArtistString()].push_back(entryIndex);
        
        // Keep hash table vectors sorted for efficient intersection
        auto& genreVec = genreIndex[entry.index->getGenreString()];
        auto& artistVec = artistIndex[entry.index->getArtistString()];
        
        std::sort(genreVec.begin(), genreVec.end());
        std::sort(artistVec.begin(), artistVec.end());
    }
}

} // namespace AudioBabel
