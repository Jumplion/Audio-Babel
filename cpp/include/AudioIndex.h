#ifndef AUDIO_INDEX_H
#define AUDIO_INDEX_H

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <gmp.h>  // GNU Multiple Precision Arithmetic Library

namespace AudioBabel {

/*
 * AudioIndex.h
 * ------------
 * Public API for the AudioIndex type. The class represents a deterministic
 * hierarchical index (genre / artist / album / track) encoded using GMP's
 * big integers and paired with a serialized fingerprint blob used for
 * reconstruction and similarity search.
 */

class AudioFingerprint; // Forward declaration (defined in AudioFingerprint.h)

class AudioIndex {
public:
    // ------------------------------------------------------------------
    // Construction / lifecycle
    // ------------------------------------------------------------------
    AudioIndex();
    AudioIndex(const AudioIndex& other);
    AudioIndex& operator=(const AudioIndex& other);
    ~AudioIndex();

    // ------------------------------------------------------------------
    // Factory functions
    // ------------------------------------------------------------------
    /**
     * Create an AudioIndex from raw PCM samples. This deterministically
     * computes a fingerprint and extracts hierarchical mpz codes.
     * @param samples PCM samples (mono interleaved)
     * @param sampleRate sample rate in Hz
     * @param bitDepth bit depth (typically 16 or 32)
     * @returns AudioIndex instance
     */
    static AudioIndex fromAudioSamples(const std::vector<int32_t>& samples, int sampleRate = 44100, int bitDepth = 16);

    /**
     * Create an AudioIndex deterministically from textual hierarchy fields.
     * Uses the printable ASCII base-94 alphabet (characters 33..126) to
     * encode each component into an mpz_t value; invalid characters will
     * cause that component to be set to zero.
     * @param genreStr Genre string
     * @param artistStr Artist string
     * @param albumStr Album string
     * @param trackStr Track string
     * @returns AudioIndex instance
     */
    static AudioIndex fromHierarchy(const std::string& genreStr, const std::string& artistStr, const std::string& albumStr, const std::string& trackStr);

    // ------------------------------------------------------------------
    // Serialization / persistence
    // ------------------------------------------------------------------
    /**
     * Writes a compact binary representation of the index to the stream.
     * Format (brief): sampleRate(int), duration(double), bitDepth(int),
     * then four mpz fields serialized as (u64 length LE + raw bytes),
     * followed by fingerprint blob as (u64 length LE + bytes).
     * @param out Output stream to write to
     */
    void serialize(std::ostream& out) const;

    /**
     * Read a serialized AudioIndex from the stream. On failure, returns an
     * AudioIndex with default fields (caller should validate contents).
     * @param in Input stream to read from
     * @returns AudioIndex instance
     */
    static AudioIndex deserialize(std::istream& in);

    // ------------------------------------------------------------------
    // Conversion / accessors
    // ------------------------------------------------------------------
    /**
     * Reconstructs PCM samples from the stored serialized fingerprint.
     * @returns an empty vector if no fingerprint is present.
     */
    std::vector<int32_t> toAudioSamples() const;

    std::string getGenreString() const;
    std::string getArtistString() const;
    std::string getAlbumString() const;
    std::string getTrackString() const;
    std::string getFullPath() const; // "genre/artist/album/track"

    // Basic properties
    int getSampleRate() const { return sampleRate; }
    double getDuration() const { return duration; }
    int getBitDepth() const { return bitDepth; }

    /**
     * Retrieve the serialized fingerprint blob used for reconstruction/search
     * @returns Fingerprint blob
     */
    const std::vector<uint8_t>& getFingerprint() const { return audioFingerprint; }

    // ------------------------------------------------------------------
    // Comparison
    // ------------------------------------------------------------------
    bool operator==(const AudioIndex& other) const;
    bool operator!=(const AudioIndex& other) const;

private:
    // ------------------------------------------------------------------
    // Internal state
    // ------------------------------------------------------------------
    mpz_t genreCode;
    mpz_t artistCode;
    mpz_t albumCode;
    mpz_t trackCode;

    int sampleRate = 44100;
    int bitDepth = 16;
    double duration = 0.0;

    // Serialized fingerprint blob (opaque to callers)
    std::vector<uint8_t> audioFingerprint;

    // ------------------------------------------------------------------
    // Internal helpers (implementation details)
    // ------------------------------------------------------------------
    void initializeMpzValues();
    void clearMpzValues();
    void copyMpzValues(const AudioIndex& other);

    /**
     * Convert a base-94 printable string into an mpz_t. Allowed characters
     * are ASCII 33..126. Returns true on success, false if the input
     * contains disallowed characters (caller can decide how to handle it).
     * @param str Input string
     * @param result Output mpz_t
     * @returns true on success, false if the input is invalid
     */
    bool stringToMpz(const std::string& str, mpz_t result) const;

    /**
     * Convert an mpz_t to a base-94 printable string (inverse of
     * stringToMpz). Zero is represented by the single character '!'
     * (ASCII 33) for determinism.
     * @param value Input mpz_t
     * @returns Base-94 printable string
     */
    std::string mpzToString(const mpz_t value) const;
};

} // namespace AudioBabel

#endif // AUDIO_INDEX_H
