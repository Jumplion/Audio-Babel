#ifndef AUDIO_BROWSER_H
#define AUDIO_BROWSER_H

/*
 * AudioBrowser.h
 * ---------------
 * Purpose: Provides deterministic and random browsing primitives over the
 *          hierarchical audio space (genre/artist/album/track). APIs return
 *          identifier strings (base-36) and AudioIndex objects constructed
 *          from those identifiers.
 */

#include "AudioIndex.h"
#include <string>
#include <vector>
#include <memory>
#include <random>

namespace AudioBabel {

/**
 * Browsing system for hierarchical navigation through the audio space
 * Mimics the Library of Babel browsing experience with genre/artist/album/track structure
 */
class AudioBrowser {
private:
    // Random number generation for browsing
    mutable std::mt19937_64 rng;
    
    // Cache for faster browsing
    struct BrowseCache {
        std::vector<std::string> cachedGenres;
        std::string lastGenre;
        std::vector<std::string> cachedArtists;
        std::string lastArtist;
        std::vector<std::string> cachedAlbums;
        bool cacheValid = false;
    };
    mutable BrowseCache cache;
    
    // Configuration
    int defaultPageSize;
    int maxCacheSize;
    
public:
    AudioBrowser();
    ~AudioBrowser();
    
    // Hierarchical browsing interface
    /**
     * Browse available genres (top level of hierarchy)
     * @param page Page number for pagination (0-based)
     * @param pageSize Number of items per page
     * @return Vector of genre identifier strings
     */
    std::vector<std::string> listGenres(int page = 0, int pageSize = 20);
    
    /**
     * Browse artists within a specific genre
     * @param genreString Genre identifier
     * @param page Page number for pagination
     * @param pageSize Number of items per page
     * @return Vector of artist identifier strings
     */
    std::vector<std::string> listArtistsInGenre(const std::string& genreString, int page = 0, int pageSize = 20);
    
    /**
     * Browse albums from a specific artist
     * @param genreString Genre identifier
     * @param artistString Artist identifier
     * @param page Page number for pagination
     * @param pageSize Number of items per page
     * @return Vector of album identifier strings
     */
    std::vector<std::string> listAlbumsFromArtist(const std::string& genreString, const std::string& artistString, int page = 0, int pageSize = 20);
    
    /**
     * Browse tracks on a specific album
     * @param genreString Genre identifier
     * @param artistString Artist identifier
     * @param albumString Album identifier
     * @param page Page number for pagination
     * @param pageSize Number of items per page
     * @return Vector of AudioIndex objects for tracks
     */
    std::vector<AudioIndex> listTracksOnAlbum(const std::string& genreString, const std::string& artistString, const std::string& albumString, int page = 0, int pageSize = 20);
    
    // Random generation (like Library of Babel's random page)
    /**
     * Generate a completely random audio track
     * @return Random AudioIndex object
     */
    AudioIndex getRandomTrack();
    
    /**
     * Generate random track within a specific genre
     * @param genreString Genre to constrain randomness to
     * @return Random AudioIndex within the genre
     */
    AudioIndex getRandomTrackInGenre(const std::string& genreString);
    
    /**
     * Generate random track from a specific artist
     * @param genreString Genre identifier
     * @param artistString Artist identifier
     * @return Random AudioIndex from the artist
     */
    AudioIndex getRandomTrackFromArtist(const std::string& genreString, const std::string& artistString);
    
    // Navigation helpers
    /**
     * Get the "next" item in the hierarchy at any level
     * Useful for sequential browsing through the space
     */
    std::string getNextGenre(const std::string& currentGenre);
    std::string getNextArtist(const std::string& genreString, const std::string& currentArtist);
    std::string getNextAlbum(const std::string& genreString, const std::string& artistString, const std::string& currentAlbum);
    AudioIndex getNextTrack(const AudioIndex& currentTrack);
    
    /**
     * Get the "previous" item in the hierarchy
     */
    std::string getPreviousGenre(const std::string& currentGenre);
    std::string getPreviousArtist(const std::string& genreString, const std::string& currentArtist);
    std::string getPreviousAlbum(const std::string& genreString, const std::string& artistString, const std::string& currentAlbum);
    AudioIndex getPreviousTrack(const AudioIndex& currentTrack);
    
    // Validation and utilities
    /**
     * Check if a hierarchical path exists/is valid
     */
    bool isValidGenre(const std::string& genreString);
    bool isValidArtist(const std::string& genreString, const std::string& artistString);
    bool isValidAlbum(const std::string& genreString, const std::string& artistString, const std::string& albumString);
    bool isValidTrack(const AudioIndex& track);
    
    /**
     * Get a specific track by its full hierarchical path
     * @param genreString Genre identifier
     * @param artistString Artist identifier  
     * @param albumString Album identifier
     * @param trackString Track identifier
     * @return AudioIndex for the specified track
     */
    AudioIndex getTrack(const std::string& genreString, const std::string& artistString, const std::string& albumString, const std::string& trackString);
    
    /**
     * Parse a full path string (e.g., "genre/artist/album/track") into components
     */
    struct HierarchicalPath {
        std::string genre;
        std::string artist;
        std::string album;
        std::string track;
        bool isValid;
    };
    HierarchicalPath parsePath(const std::string& fullPath);
    
    // Configuration
    void setDefaultPageSize(int pageSize) { defaultPageSize = pageSize; }
    void setMaxCacheSize(int cacheSize) { maxCacheSize = cacheSize; }
    void clearCache() { cache.cacheValid = false; }
    
    // Statistics
    struct BrowseStats {
        size_t estimatedGenres;
        size_t estimatedArtists;
        size_t estimatedAlbums;
        size_t estimatedTracks;
    };
    BrowseStats getEstimatedCounts();
    
private:
    // Internal generation methods
    std::string generateGenreAtIndex(size_t index);
    std::string generateArtistAtIndex(const std::string& genreString, size_t index);
    std::string generateAlbumAtIndex(const std::string& genreString, const std::string& artistString, size_t index);
    AudioIndex generateTrackAtIndex(const std::string& genreString, const std::string& artistString, const std::string& albumString, size_t index);

    // Cache management
    void updateGenreCache() const;
    void updateArtistCache(const std::string& genreString) const;
    void updateAlbumCache(const std::string& genreString, const std::string& artistString) const;
    
    // String encoding/decoding for identifiers
    std::string encodeIdentifier(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decodeIdentifier(const std::string& identifier);
};

} // namespace AudioBabel

/* Known issues / suggested fixes:
 *  - `encodeIdentifier`/`decodeIdentifier` assume base-36 round-trip and do not validate
 *    malformed identifiers. Add validation and clear error handling for mpz_set_str.
 *  - RNG seeding with wall-clock time makes generated tracks non-reproducible.
 *    If reproducible browsing is required, allow injecting a seed.
 *  - Byte-order and leading-zero handling in getNext/getPrevious helpers should be
 *    documented and tested for edge cases (single-byte identifiers, overflow).
 */

#endif // AUDIO_BROWSER_H
