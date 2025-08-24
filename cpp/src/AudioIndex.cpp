#include "AudioIndex.h"
#include "AudioFingerprint.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>

namespace AudioBabel {

/*
 * AudioIndex.cpp
 * ----------------
 * Implementation notes:
 *  - Responsible for constructing AudioIndex values from PCM or hierarchical
 *    identifiers, serializing to/from streams, and simple deterministic
 *    neighborhood generation for browsing.
 *
 * Quick warnings:
 *  - `fromHierarchy` currently writes raw mpz_export bytes into `audioFingerprint`.
 *    This is not the same format produced by `AudioFingerprint::serialize()` and will
 *    cause `AudioFingerprint::deserialize()` to fail if `toAudioSamples()` is called.
 *  - mpz string conversions use base-36 without validation; invalid input will
 *    set GMP's internal error state. Validate inputs at call sites.
 */
AudioIndex::AudioIndex() : sampleRate(44100), duration(0.0), bitDepth(16) {
    initializeMpzValues();
}

AudioIndex::AudioIndex(const AudioIndex& other) 
    : sampleRate(other.sampleRate), duration(other.duration), bitDepth(other.bitDepth),
        audioFingerprint(other.audioFingerprint) {
    initializeMpzValues();
    copyMpzValues(other);
}

AudioIndex& AudioIndex::operator=(const AudioIndex& other) {
    if (this != &other) {
        sampleRate = other.sampleRate;
    duration = other.duration;
        bitDepth = other.bitDepth;
        audioFingerprint = other.audioFingerprint;
        copyMpzValues(other);
    }
    return *this;
}

AudioIndex::~AudioIndex() {
    clearMpzValues();
}

void AudioIndex::initializeMpzValues() {
    mpz_init(genreCode);
    mpz_init(artistCode);
    mpz_init(albumCode);
    mpz_init(trackCode);
}


void AudioIndex::clearMpzValues() {
    mpz_clear(genreCode);
    mpz_clear(artistCode);
    mpz_clear(albumCode);
    mpz_clear(trackCode);
}

void AudioIndex::copyMpzValues(const AudioIndex& other) {
    mpz_set(genreCode, other.genreCode);
    mpz_set(artistCode, other.artistCode);
    mpz_set(albumCode, other.albumCode);
    mpz_set(trackCode, other.trackCode);
}

AudioIndex AudioIndex::fromAudioSamples(const std::vector<int32_t>& samples, int sampleRate, int bitDepth) {
    AudioIndex index;
    index.sampleRate = sampleRate;
    index.bitDepth = bitDepth;
    // Duration in seconds; guard against division by zero and allow fractional seconds
    if (sampleRate > 0) {
        index.duration = static_cast<double>(samples.size()) / static_cast<double>(sampleRate);
    } else {
        index.duration = 0.0;
    }
    
    // Generate fingerprint from samples
    AudioFingerprint fingerprint = AudioFingerprint::fromSamples(samples, sampleRate);
    index.audioFingerprint = fingerprint.serialize();
    
    // Extract hierarchical codes from fingerprint
    fingerprint.extractCodes(index.genreCode, index.artistCode, index.albumCode, index.trackCode);
    
    return index;
}

AudioIndex AudioIndex::fromHierarchy(const std::string& genreStr, const std::string& artistStr, const std::string& albumStr, const std::string& trackStr) {
    AudioIndex index;
    
    // Convert strings to mpz values
    index.stringToMpz(genreStr, index.genreCode);
    index.stringToMpz(artistStr, index.artistCode);
    index.stringToMpz(albumStr, index.albumCode);
    index.stringToMpz(trackStr, index.trackCode);
    
    // Generate audio fingerprint from the hierarchical codes
    // This is a deterministic process that creates audio from the codes
    std::vector<uint8_t> combinedData;
    
    // Serialize mpz values and combine
    size_t genreSize = mpz_sizeinbase(index.genreCode, 256);
    size_t artistSize = mpz_sizeinbase(index.artistCode, 256);
    size_t albumSize = mpz_sizeinbase(index.albumCode, 256);
    size_t trackSize = mpz_sizeinbase(index.trackCode, 256);
    
    combinedData.resize(genreSize + artistSize + albumSize + trackSize);
    
    size_t offset = 0;
    mpz_export(combinedData.data() + offset, nullptr, 1, 1, 0, 0, index.genreCode);
    offset += genreSize;
    mpz_export(combinedData.data() + offset, nullptr, 1, 1, 0, 0, index.artistCode);
    offset += artistSize;
    mpz_export(combinedData.data() + offset, nullptr, 1, 1, 0, 0, index.albumCode);
    offset += albumSize;
    mpz_export(combinedData.data() + offset, nullptr, 1, 1, 0, 0, index.trackCode);
    
    // Convert combined mpz bytes into the AudioFingerprint serialized format so
    // AudioFingerprint::deserialize() can reconstruct a fingerprint deterministically.
    // AudioFingerprint::serialize() layout: int originalSampleRate, int originalDuration, uint32_t numBlocks, followed by numBlocks * FREQUENCY_BANDS bytes.
    const size_t FREQUENCY_BANDS = 32; // must match AudioFingerprint implementation
    size_t numBlocks = (combinedData.size() + FREQUENCY_BANDS - 1) / FREQUENCY_BANDS;
    if (numBlocks == 0) numBlocks = 1; // ensure at least one block

    // Pad combinedData to fit whole blocks
    size_t paddedSize = numBlocks * FREQUENCY_BANDS;
    combinedData.resize(paddedSize, 0);

    std::vector<uint8_t> serialized;
    // originalSampleRate (int)
    int originalSampleRate = index.sampleRate;
    serialized.insert(serialized.end(), reinterpret_cast<uint8_t*>(&originalSampleRate), reinterpret_cast<uint8_t*>(&originalSampleRate) + sizeof(originalSampleRate));
    // originalDuration (int) - unknown, set to 0
    int originalDuration = 0;
    serialized.insert(serialized.end(), reinterpret_cast<uint8_t*>(&originalDuration), reinterpret_cast<uint8_t*>(&originalDuration) + sizeof(originalDuration));
    // numBlocks (uint32_t)
    uint32_t nb = static_cast<uint32_t>(numBlocks);
    serialized.insert(serialized.end(), reinterpret_cast<uint8_t*>(&nb), reinterpret_cast<uint8_t*>(&nb) + sizeof(nb));

    // Append block data
    serialized.insert(serialized.end(), combinedData.begin(), combinedData.end());

    // Wrap with magic + version to match AudioFingerprint::serialize() output
    const uint8_t magic[4] = { 'A', 'F', 'P', 'B' };
    const uint8_t version = 0x01;
    std::vector<uint8_t> wrapped;
    wrapped.insert(wrapped.end(), std::begin(magic), std::end(magic));
    wrapped.push_back(version);
    wrapped.insert(wrapped.end(), serialized.begin(), serialized.end());

    index.audioFingerprint = std::move(wrapped);
    
    return index;
}

std::vector<int32_t> AudioIndex::toAudioSamples() const {
    if (audioFingerprint.empty()) {
        return std::vector<int32_t>();
    }
    
    // Deserialize fingerprint and convert to samples
    AudioFingerprint fingerprint = AudioFingerprint::deserialize(audioFingerprint);
    return fingerprint.toSamples(sampleRate);
}

void AudioIndex::serialize(std::ostream& out) const {
    // Write basic properties
    out.write(reinterpret_cast<const char*>(&sampleRate), sizeof(sampleRate));
    out.write(reinterpret_cast<const char*>(&duration), sizeof(duration));
    out.write(reinterpret_cast<const char*>(&bitDepth), sizeof(bitDepth));
    
    // Write mpz values
    auto writeMpz = [&out](const mpz_t value) {
        size_t size = mpz_sizeinbase(value, 256);
        out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        if (size > 0) {
            std::vector<uint8_t> buffer(size);
            mpz_export(buffer.data(), nullptr, 1, 1, 0, 0, value);
            out.write(reinterpret_cast<const char*>(buffer.data()), size);
        }
    };
    
    writeMpz(genreCode);
    writeMpz(artistCode);
    writeMpz(albumCode);
    writeMpz(trackCode);
    
    // Write fingerprint
    size_t fingerprintSize = audioFingerprint.size();
    out.write(reinterpret_cast<const char*>(&fingerprintSize), sizeof(fingerprintSize));
    if (fingerprintSize > 0) {
        out.write(reinterpret_cast<const char*>(audioFingerprint.data()), fingerprintSize);
    }
}

AudioIndex AudioIndex::deserialize(std::istream& in) {
    AudioIndex index;
    
    // Read basic properties
    in.read(reinterpret_cast<char*>(&index.sampleRate), sizeof(index.sampleRate));
    in.read(reinterpret_cast<char*>(&index.duration), sizeof(index.duration));
    in.read(reinterpret_cast<char*>(&index.bitDepth), sizeof(index.bitDepth));
    
    // Read mpz values
    auto readMpz = [&in](mpz_t value) {
        size_t size;
        in.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (size > 0) {
            std::vector<uint8_t> buffer(size);
            in.read(reinterpret_cast<char*>(buffer.data()), size);
            mpz_import(value, size, 1, 1, 0, 0, buffer.data());
        }
    };
    
    readMpz(index.genreCode);
    readMpz(index.artistCode);
    readMpz(index.albumCode);
    readMpz(index.trackCode);
    
    // Read fingerprint
    size_t fingerprintSize;
    in.read(reinterpret_cast<char*>(&fingerprintSize), sizeof(fingerprintSize));
    if (fingerprintSize > 0) {
        index.audioFingerprint.resize(fingerprintSize);
        in.read(reinterpret_cast<char*>(index.audioFingerprint.data()), fingerprintSize);
    }
    
    return index;
}

std::string AudioIndex::getGenreString() const {
    return mpzToString(genreCode);
}

std::string AudioIndex::getArtistString() const {
    return mpzToString(artistCode);
}

std::string AudioIndex::getAlbumString() const {
    return mpzToString(albumCode);
}

std::string AudioIndex::getTrackString() const {
    return mpzToString(trackCode);
}

std::string AudioIndex::getFullPath() const {
    return getGenreString() + "/" + getArtistString() + "/" + getAlbumString() + "/" + getTrackString();
}

void AudioIndex::stringToMpz(const std::string& str, mpz_t result) const {
    // Convert string to base-36 representation, then to mpz
    mpz_set_str(result, str.c_str(), 36);
}

std::string AudioIndex::mpzToString(const mpz_t value) const {
    // Convert mpz to base-36 string representation
    char* str = mpz_get_str(nullptr, 36, value);
    std::string result(str);
    free(str);
    return result;
}

/*
std::vector<AudioIndex> AudioIndex::getSimilarGenres(int count) const {
    std::vector<AudioIndex> result;
    std::mt19937_64 rng(mpz_get_ui(genreCode));
    
    for (int i = 0; i < count; ++i) {
        mpz_t newGenre;
        mpz_init(newGenre);
        
        // Generate similar genre code by adding small random offsets
        mpz_t offset;
        mpz_init(offset);
        mpz_set_ui(offset, rng() % 1000);
        mpz_add(newGenre, genreCode, offset);
        
        AudioIndex similar = *this;
        mpz_set(similar.genreCode, newGenre);
        result.push_back(similar);
        
        mpz_clear(newGenre);
        mpz_clear(offset);
    }
    
    return result;
}

std::vector<AudioIndex> AudioIndex::getArtistsInGenre(int count) const {
    std::vector<AudioIndex> result;
    std::mt19937_64 rng(mpz_get_ui(genreCode) + mpz_get_ui(artistCode));
    
    for (int i = 0; i < count; ++i) {
        mpz_t newArtist;
        mpz_init(newArtist);
        
        // Generate different artist in same genre
        mpz_t offset;
        mpz_init(offset);
        mpz_set_ui(offset, rng() % 10000);
        mpz_add(newArtist, artistCode, offset);
        
        AudioIndex similar = *this;
        mpz_set(similar.artistCode, newArtist);
        result.push_back(similar);
        
        mpz_clear(newArtist);
        mpz_clear(offset);
    }
    
    return result;
}

std::vector<AudioIndex> AudioIndex::getAlbumsFromArtist(int count) const {
    std::vector<AudioIndex> result;
    std::mt19937_64 rng(mpz_get_ui(artistCode) + mpz_get_ui(albumCode));
    
    for (int i = 0; i < count; ++i) {
        mpz_t newAlbum;
        mpz_init(newAlbum);
        
        // Generate different album from same artist
        mpz_t offset;
        mpz_init(offset);
        mpz_set_ui(offset, rng() % 100);
        mpz_add(newAlbum, albumCode, offset);
        
        AudioIndex similar = *this;
        mpz_set(similar.albumCode, newAlbum);
        result.push_back(similar);
        
        mpz_clear(newAlbum);
        mpz_clear(offset);
    }
    
    return result;
}

std::vector<AudioIndex> AudioIndex::getTracksFromAlbum(int count) const {
    std::vector<AudioIndex> result;
    std::mt19937_64 rng(mpz_get_ui(albumCode) + mpz_get_ui(trackCode));
    
    for (int i = 0; i < count; ++i) {
        mpz_t newTrack;
        mpz_init(newTrack);
        
        // Generate different track from same album
        mpz_t offset;
        mpz_init(offset);
        mpz_set_ui(offset, rng() % 20);
        mpz_add(newTrack, trackCode, offset);
        
        AudioIndex similar = *this;
        mpz_set(similar.trackCode, newTrack);
        result.push_back(similar);
        
        mpz_clear(newTrack);
        mpz_clear(offset);
    }
    
    return result;
}
*/

bool AudioIndex::operator==(const AudioIndex& other) const {
    return mpz_cmp(genreCode, other.genreCode) == 0 &&
        mpz_cmp(artistCode, other.artistCode) == 0 &&
        mpz_cmp(albumCode, other.albumCode) == 0 &&
        mpz_cmp(trackCode, other.trackCode) == 0;
}

bool AudioIndex::operator!=(const AudioIndex& other) const {
    return !(*this == other);
}

} // namespace AudioBabel
