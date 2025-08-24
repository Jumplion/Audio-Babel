#ifndef AUDIO_INDEX_H
#define AUDIO_INDEX_H

/*
 * AudioIndex.h
 * ----------------
 * Purpose: Represents a deterministic, hierarchical audio index (genre/artist/album/track)
 *          used by the Speaker-of-Babel prototype. The class stores GMP big-integer
 *          codes for each level and a serialized fingerprint blob for reconstruction
 *          and search.
 *
 * Notes:
 *  - This header documents the public API only; implementations live in AudioIndex.cpp.
 *  - The index is intended to be deterministic for a given hierarchy or audio input.
 */

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <gmp.h>  // GNU Multiple Precision Arithmetic Library

namespace AudioBabel {

class AudioFingerprint; // Forward declaration

/**
 * Represents a hierarchical audio index with genre/artist/album/track structure
 * Similar to Library of Babel's hexagon-wall-shelf-volume system
 */
class AudioIndex {
private:
    // Hierarchical components (stored as large integers)
    mpz_t genreCode;
    mpz_t artistCode;
    mpz_t albumCode;
    mpz_t trackCode;
    
    // Audio properties
    int sampleRate = 44100;
    int bitDepth = 16;
    double duration;
    
    // Audio fingerprint for reconstruction and search
    std::vector<uint8_t> audioFingerprint;
    
public:
    AudioIndex();
    AudioIndex(const AudioIndex& other);
    AudioIndex& operator=(const AudioIndex& other);
    ~AudioIndex();
    
    // Factory methods
    /**
     * Creates index from raw audio samples
     * @param samples PCM audio samples
     * @param sampleRate Sample rate in Hz (Default: 44100)
     * @param bitDepth Bit depth (Default: 16)
     * @return AudioIndex object
     */
    static AudioIndex fromAudioSamples(const std::vector<int32_t>& samples, int sampleRate = 44100, int bitDepth = 16);

    /**
     * Creates index from hierarchical string identifiers
     * String identifiers can be made up of numbers, uppercase, and lowercase letters.
     * @param genreStr Genre identifier string
     * @param artistStr Artist identifier string
     * @param albumStr Album identifier string
     * @param trackStr Track identifier string
     * @return AudioIndex object
     */
    static AudioIndex fromHierarchy(const std::string& genreStr, const std::string& artistStr, const std::string& albumStr, const std::string& trackStr);
    
    // Audio reconstruction
    /**
     * Reconstructs audio samples from the index
     * @return Vector of PCM samples
     */
    std::vector<int32_t> toAudioSamples() const;
    
    // Serialization
    void serialize(std::ostream& out) const;
    static AudioIndex deserialize(std::istream& in);
    
    // Human-readable representations
    std::string getGenreString() const;
    std::string getArtistString() const;
    std::string getAlbumString() const;
    std::string getTrackString() const;
    std::string getFullPath() const; // genre/artist/album/track
    
    // Properties
    int getSampleRate() const { return sampleRate; }
    double getDuration() const { return duration; }
    int getBitDepth() const { return bitDepth; }
    
    // Navigation helpers for browsing
    // std::vector<AudioIndex> getSimilarGenres(int count = 10) const;
    // std::vector<AudioIndex> getArtistsInGenre(int count = 20) const;
    // std::vector<AudioIndex> getAlbumsFromArtist(int count = 15) const;
    // std::vector<AudioIndex> getTracksFromAlbum(int count = 12) const;
    
    // Comparison operators
    bool operator==(const AudioIndex& other) const;
    bool operator!=(const AudioIndex& other) const;
    
    // Get fingerprint for search operations
    const std::vector<uint8_t>& getFingerprint() const { return audioFingerprint; }
    
private:
    // Helper methods for managing mpz_t values
    void initializeMpzValues();
    void clearMpzValues();
    void copyMpzValues(const AudioIndex& other);
    
    // Helper methods for string to mpz conversion
    void stringToMpz(const std::string& str, mpz_t result) const;
    std::string mpzToString(const mpz_t value) const;
};

} // namespace AudioBabel

/* Known issues / suggested fixes:
 *  - Serialization uses `size_t` and native endianness for lengths; this is non-portable.
 *    Use fixed-width integer types (uint32_t/uint64_t) and define endianness.
 *  - `stringToMpz` uses `mpz_set_str(..., 36)` without input validation; invalid strings
 *    will set an error state. Validate inputs and handle exceptions/return errors.
 *  - Many methods assume non-empty fingerprints; guard against empty/short audio inputs.
 */

#endif // AUDIO_INDEX_H
