#include "AudioBrowser.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <chrono>

namespace AudioBabel {

/*
 * AudioBrowser.cpp
 * -----------------
 * Purpose: Implements deterministic generation and navigation helpers for the
 *          Library-of-Babel-style browsing over hierarchical identifiers.
 *
 * Notes:
 *  - Random generation uses std::mt19937_64 seeded from wall-clock time; make
 *    the seed injectable for reproducible tests.
 *  - encode/decode routines rely on GMP base-36 conversions; invalid identifiers
 *    should be handled gracefully.
 */
AudioBrowser::AudioBrowser() : defaultPageSize(20), maxCacheSize(1000) {
    // Initialize random number generator with current time
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    rng.seed(seed);
}

AudioBrowser::~AudioBrowser() {
}

std::vector<std::string> AudioBrowser::listGenres(int page, int pageSize) {
    std::vector<std::string> genres;
    genres.reserve(pageSize);
    
    // Generate genre identifiers for the requested page
    size_t startIndex = page * pageSize;
    for (int i = 0; i < pageSize; ++i) {
        std::string genre = generateGenreAtIndex(startIndex + i);
        genres.push_back(genre);
    }
    
    return genres;
}

std::vector<std::string> AudioBrowser::listArtistsInGenre(const std::string& genreString, 
        int page, int pageSize) {
    std::vector<std::string> artists;
    artists.reserve(pageSize);
    
    size_t startIndex = page * pageSize;
    for (int i = 0; i < pageSize; ++i) {
        std::string artist = generateArtistAtIndex(genreString, startIndex + i);
        artists.push_back(artist);
    }
    
    return artists;
}

std::vector<std::string> AudioBrowser::listAlbumsFromArtist(const std::string& genreString,
        const std::string& artistString, int page, int pageSize) {
    std::vector<std::string> albums;
    albums.reserve(pageSize);
    
    size_t startIndex = page * pageSize;
    for (int i = 0; i < pageSize; ++i) {
        std::string album = generateAlbumAtIndex(genreString, artistString, startIndex + i);
        albums.push_back(album);
    }
    
    return albums;
}

std::vector<AudioIndex> AudioBrowser::listTracksOnAlbum(const std::string& genreString,
        const std::string& artistString, const std::string& albumString,
        int page, int pageSize) {
    std::vector<AudioIndex> tracks;
    tracks.reserve(pageSize);
    
    size_t startIndex = page * pageSize;
    for (int i = 0; i < pageSize; ++i) {
        AudioIndex track = generateTrackAtIndex(genreString, artistString, albumString, startIndex + i);
        tracks.push_back(track);
    }
    
    return tracks;
}

AudioIndex AudioBrowser::getRandomTrack() {
    // Generate completely random hierarchical identifiers
    std::uniform_int_distribution<uint64_t> dist;
    
    uint64_t genreRand = dist(rng);
    uint64_t artistRand = dist(rng);
    uint64_t albumRand = dist(rng);
    uint64_t trackRand = dist(rng);
    
    std::string genreStr = encodeIdentifier({
        static_cast<uint8_t>(genreRand >> 56),
        static_cast<uint8_t>(genreRand >> 48),
        static_cast<uint8_t>(genreRand >> 40),
        static_cast<uint8_t>(genreRand >> 32),
        static_cast<uint8_t>(genreRand >> 24),
        static_cast<uint8_t>(genreRand >> 16),
        static_cast<uint8_t>(genreRand >> 8),
        static_cast<uint8_t>(genreRand)
    });
    
    std::string artistStr = encodeIdentifier({
        static_cast<uint8_t>(artistRand >> 56),
        static_cast<uint8_t>(artistRand >> 48),
        static_cast<uint8_t>(artistRand >> 40),
        static_cast<uint8_t>(artistRand >> 32),
        static_cast<uint8_t>(artistRand >> 24),
        static_cast<uint8_t>(artistRand >> 16),
        static_cast<uint8_t>(artistRand >> 8),
        static_cast<uint8_t>(artistRand)
    });
    
    std::string albumStr = encodeIdentifier({
        static_cast<uint8_t>(albumRand >> 24),
        static_cast<uint8_t>(albumRand >> 16),
        static_cast<uint8_t>(albumRand >> 8),
        static_cast<uint8_t>(albumRand)
    });
    
    std::string trackStr = encodeIdentifier({
        static_cast<uint8_t>(trackRand >> 24),
        static_cast<uint8_t>(trackRand >> 16),
        static_cast<uint8_t>(trackRand >> 8),
        static_cast<uint8_t>(trackRand)
    });
    
    return AudioIndex::fromHierarchy(genreStr, artistStr, albumStr, trackStr);
}

AudioIndex AudioBrowser::getRandomTrackInGenre(const std::string& genreString) {
    std::uniform_int_distribution<uint64_t> dist;
    
    uint64_t artistRand = dist(rng);
    uint64_t albumRand = dist(rng);
    uint64_t trackRand = dist(rng);
    
    std::string artistStr = encodeIdentifier({
        static_cast<uint8_t>(artistRand >> 56),
        static_cast<uint8_t>(artistRand >> 48),
        static_cast<uint8_t>(artistRand >> 40),
        static_cast<uint8_t>(artistRand >> 32),
        static_cast<uint8_t>(artistRand >> 24),
        static_cast<uint8_t>(artistRand >> 16),
        static_cast<uint8_t>(artistRand >> 8),
        static_cast<uint8_t>(artistRand)
    });
    
    std::string albumStr = encodeIdentifier({
        static_cast<uint8_t>(albumRand >> 24),
        static_cast<uint8_t>(albumRand >> 16),
        static_cast<uint8_t>(albumRand >> 8),
        static_cast<uint8_t>(albumRand)
    });
    
    std::string trackStr = encodeIdentifier({
        static_cast<uint8_t>(trackRand >> 24),
        static_cast<uint8_t>(trackRand >> 16),
        static_cast<uint8_t>(trackRand >> 8),
        static_cast<uint8_t>(trackRand)
    });
    
    return AudioIndex::fromHierarchy(genreString, artistStr, albumStr, trackStr);
}

AudioIndex AudioBrowser::getRandomTrackFromArtist(const std::string& genreString, const std::string& artistString) {
    std::uniform_int_distribution<uint64_t> dist;
    
    uint64_t albumRand = dist(rng);
    uint64_t trackRand = dist(rng);
    
    std::string albumStr = encodeIdentifier({
        static_cast<uint8_t>(albumRand >> 24),
        static_cast<uint8_t>(albumRand >> 16),
        static_cast<uint8_t>(albumRand >> 8),
        static_cast<uint8_t>(albumRand)
    });
    
    std::string trackStr = encodeIdentifier({
        static_cast<uint8_t>(trackRand >> 24),
        static_cast<uint8_t>(trackRand >> 16),
        static_cast<uint8_t>(trackRand >> 8),
        static_cast<uint8_t>(trackRand)
    });
    
    return AudioIndex::fromHierarchy(genreString, artistString, albumStr, trackStr);
}

std::string AudioBrowser::getNextGenre(const std::string& currentGenre) {
    // Decode current genre, increment, and re-encode
    std::vector<uint8_t> data = decodeIdentifier(currentGenre);
    
    // Increment the byte array (big-endian)
    bool carry = true;
    for (int i = data.size() - 1; i >= 0 && carry; --i) {
        if (data[i] == 255) {
            data[i] = 0;
        } else {
            data[i]++;
            carry = false;
        }
    }
    
    if (carry) {
        // Overflow, add a new byte
        data.insert(data.begin(), 1);
    }
    
    return encodeIdentifier(data);
}

std::string AudioBrowser::getNextArtist(const std::string& genreString, const std::string& currentArtist) {
    std::vector<uint8_t> data = decodeIdentifier(currentArtist);
    
    bool carry = true;
    for (int i = data.size() - 1; i >= 0 && carry; --i) {
        if (data[i] == 255) {
            data[i] = 0;
        } else {
            data[i]++;
            carry = false;
        }
    }
    
    if (carry) {
        data.insert(data.begin(), 1);
    }
    
    return encodeIdentifier(data);
}

std::string AudioBrowser::getNextAlbum(const std::string& genreString, const std::string& artistString, const std::string& currentAlbum) {
    std::vector<uint8_t> data = decodeIdentifier(currentAlbum);
    
    bool carry = true;
    for (int i = data.size() - 1; i >= 0 && carry; --i) {
        if (data[i] == 255) {
            data[i] = 0;
        } else {
            data[i]++;
            carry = false;
        }
    }
    
    if (carry) {
        data.insert(data.begin(), 1);
    }
    
    return encodeIdentifier(data);
}

AudioIndex AudioBrowser::getNextTrack(const AudioIndex& currentTrack) {
    std::string nextTrackStr = getNextAlbum("", "", currentTrack.getTrackString());
    return AudioIndex::fromHierarchy(
        currentTrack.getGenreString(),
        currentTrack.getArtistString(),
        currentTrack.getAlbumString(),
        nextTrackStr
    );
}

std::string AudioBrowser::getPreviousGenre(const std::string& currentGenre) {
    std::vector<uint8_t> data = decodeIdentifier(currentGenre);
    
    // Decrement the byte array (big-endian)
    bool borrow = true;
    for (int i = data.size() - 1; i >= 0 && borrow; --i) {
        if (data[i] == 0) {
            data[i] = 255;
        } else {
            data[i]--;
            borrow = false;
        }
    }
    
    // Remove leading zeros
    while (data.size() > 1 && data[0] == 0) {
        data.erase(data.begin());
    }
    
    return encodeIdentifier(data);
}

std::string AudioBrowser::getPreviousArtist(const std::string& genreString, const std::string& currentArtist) {
    std::vector<uint8_t> data = decodeIdentifier(currentArtist);
    
    bool borrow = true;
    for (int i = data.size() - 1; i >= 0 && borrow; --i) {
        if (data[i] == 0) {
            data[i] = 255;
        } else {
            data[i]--;
            borrow = false;
        }
    }
    
    while (data.size() > 1 && data[0] == 0) {
        data.erase(data.begin());
    }
    
    return encodeIdentifier(data);
}

std::string AudioBrowser::getPreviousAlbum(const std::string& genreString, const std::string& artistString, const std::string& currentAlbum) {
    std::vector<uint8_t> data = decodeIdentifier(currentAlbum);
    
    bool borrow = true;
    for (int i = data.size() - 1; i >= 0 && borrow; --i) {
        if (data[i] == 0) {
            data[i] = 255;
        } else {
            data[i]--;
            borrow = false;
        }
    }
    
    while (data.size() > 1 && data[0] == 0) {
        data.erase(data.begin());
    }
    
    return encodeIdentifier(data);
}

AudioIndex AudioBrowser::getPreviousTrack(const AudioIndex& currentTrack) {
    std::string prevTrackStr = getPreviousAlbum("", "", currentTrack.getTrackString());
    return AudioIndex::fromHierarchy(
        currentTrack.getGenreString(),
        currentTrack.getArtistString(),
        currentTrack.getAlbumString(),
        prevTrackStr
    );
}

bool AudioBrowser::isValidGenre(const std::string& genreString) {
    // All non-empty strings are valid genres
    return !genreString.empty();
}

bool AudioBrowser::isValidArtist(const std::string& genreString, const std::string& artistString) {
    return !genreString.empty() && !artistString.empty();
}

bool AudioBrowser::isValidAlbum(const std::string& genreString, const std::string& artistString, const std::string& albumString) {
    return !genreString.empty() && !artistString.empty() && !albumString.empty();
}

bool AudioBrowser::isValidTrack(const AudioIndex& track) {
    return !track.getGenreString().empty() && 
        !track.getArtistString().empty() && 
        !track.getAlbumString().empty() && 
        !track.getTrackString().empty();
}

AudioIndex AudioBrowser::getTrack(const std::string& genreString, const std::string& artistString, const std::string& albumString, const std::string& trackString) {
    return AudioIndex::fromHierarchy(genreString, artistString, albumString, trackString);
}

AudioBrowser::HierarchicalPath AudioBrowser::parsePath(const std::string& fullPath) {
    HierarchicalPath path;
    path.isValid = false;
    
    std::istringstream ss(fullPath);
    std::string segment;
    std::vector<std::string> segments;
    
    while (std::getline(ss, segment, '/')) {
        if (!segment.empty()) {
            segments.push_back(segment);
        }
    }
    
    if (segments.size() == 4) {
        path.genre = segments[0];
        path.artist = segments[1];
        path.album = segments[2];
        path.track = segments[3];
        path.isValid = true;
    }
    
    return path;
}

AudioBrowser::BrowseStats AudioBrowser::getEstimatedCounts() {
    BrowseStats stats;
    
    // These are rough estimates based on the addressing space
    // In reality, the space is much larger than these numbers suggest
    stats.estimatedGenres = 1000000;     // 1 million genres
    stats.estimatedArtists = 100000000;  // 100 million artists per genre
    stats.estimatedAlbums = 10000;       // 10k albums per artist
    stats.estimatedTracks = 20;          // 20 tracks per album on average
    
    return stats;
}

// Private methods
std::string AudioBrowser::generateGenreAtIndex(size_t index) {
    // Generate deterministic genre string from index
    std::vector<uint8_t> data;
    
    if (index == 0) {
        data.push_back(0);
    } else {
        while (index > 0) {
            data.insert(data.begin(), static_cast<uint8_t>(index & 0xFF));
            index >>= 8;
        }
    }
    
    return encodeIdentifier(data);
}

std::string AudioBrowser::generateArtistAtIndex(const std::string& genreString, size_t index) {
    // Combine genre hash with index for deterministic but genre-dependent artists
    std::hash<std::string> hasher;
    size_t genreHash = hasher(genreString);
    size_t combinedIndex = genreHash ^ index;
    
    return generateGenreAtIndex(combinedIndex);
}

std::string AudioBrowser::generateAlbumAtIndex(const std::string& genreString, const std::string& artistString, size_t index) {
    std::hash<std::string> hasher;
    size_t combinedHash = hasher(genreString + artistString);
    size_t combinedIndex = combinedHash ^ index;
    
    return generateGenreAtIndex(combinedIndex);
}

AudioIndex AudioBrowser::generateTrackAtIndex(const std::string& genreString, const std::string& artistString, const std::string& albumString, size_t index) {
    std::hash<std::string> hasher;
    size_t combinedHash = hasher(genreString + artistString + albumString);
    size_t trackIndex = combinedHash ^ index;
    
    std::string trackStr = generateGenreAtIndex(trackIndex);
    
    return AudioIndex::fromHierarchy(genreString, artistString, albumString, trackStr);
}

std::string AudioBrowser::encodeIdentifier(const std::vector<uint8_t>& data) {
    // Use base-36 encoding for aesthetic similarity to Library of Babel
    if (data.empty()) {
        return "0";
    }
    
    // Convert byte array to large integer, then to base-36 string
    mpz_t num;
    mpz_init(num);
    mpz_import(num, data.size(), 1, 1, 0, 0, data.data());
    
    char* str = mpz_get_str(nullptr, 36, num);
    std::string result(str);
    free(str);
    mpz_clear(num);
    
    return result;
}

std::vector<uint8_t> AudioBrowser::decodeIdentifier(const std::string& identifier) {
    mpz_t num;
    mpz_init(num);
    mpz_set_str(num, identifier.c_str(), 36);
    
    size_t byteCount = (mpz_sizeinbase(num, 2) + 7) / 8;
    if (byteCount == 0) byteCount = 1;
    
    std::vector<uint8_t> result(byteCount > 0 ? byteCount : 1);
    size_t actual = 0;
    mpz_export(result.data(), &actual, 1, 1, 0, 0, num);
    if (actual > 0) result.resize(actual);
    
    mpz_clear(num);
    return result;
}

} // namespace AudioBabel
