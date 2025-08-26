#ifndef AUDIO_FINGERPRINT_H
#define AUDIO_FINGERPRINT_H

/*
 * AudioFingerprint.h
 * -------------------
 * Purpose: Define a compact, perceptual time-frequency fingerprint used to
 *          represent audio for browsing and search. The fingerprint uses
 *          fixed-size time blocks and a small number of frequency bands.
 *
 * High-level contract:
 *  - Inputs: PCM samples (int32_t vector) and sample rate
 *  - Outputs: quantized time-frequency blocks and serialized blob
 *  - Error modes: empty or too-short input returns an empty fingerprint
 */

#include <vector>
#include <complex>
#include <cstdint>
#include <gmp.h>

namespace AudioBabel {

/**
 * Audio fingerprinting system for perceptual audio representation
 * Divides audio into time-frequency blocks for compact representation
 */
class AudioFingerprint {
private:
    static constexpr int BLOCK_SIZE_MS = 100;           // 100ms blocks
    static constexpr int FREQUENCY_BANDS = 32;          // Number of frequency bands
    static constexpr int OVERLAP_PERCENT = 50;          // Block overlap percentage
    static constexpr double MIN_FREQUENCY = 80.0;       // Minimum frequency (Hz)
    static constexpr double MAX_FREQUENCY = 8000.0;     // Maximum frequency (Hz)
    
    /**
     * Time-frequency representation
     * Each inner vector represents frequency bands for one time block
     */
    std::vector<std::vector<uint8_t>> timeFrequencyBlocks;
    
    int originalSampleRate;
    int originalDuration;
    
public:
    AudioFingerprint();
    AudioFingerprint(const AudioFingerprint& other);
    AudioFingerprint& operator=(const AudioFingerprint& other);
    ~AudioFingerprint();
    
    /**
     * Generate fingerprint from raw PCM data
     * @param samples PCM audio samples
     * @param sampleRate Sample rate in Hz
     * @return AudioFingerprint object
     */
    static AudioFingerprint fromSamples(const std::vector<int32_t>& samples, int sampleRate);
    
    /**
     * Regenerate approximate audio from fingerprint
     * Uses spectral reconstruction techniques
     * @param sampleRate Desired output sample rate
     * @return Vector of reconstructed PCM samples
     */
    std::vector<int32_t> toSamples(int sampleRate) const;
    
    /**
     * Extract hierarchical codes from fingerprint for browsing system
     * Uses different regions of the fingerprint for each level
     * @param genreCode Output parameter for genre code
     * @param artistCode Output parameter for artist code
     * @param albumCode Output parameter for album code
     * @param trackCode Output parameter for track code
     */
    void extractCodes(mpz_t genreCode, mpz_t artistCode, mpz_t albumCode, mpz_t trackCode) const;
    
    /**
     * Check if this fingerprint contains another (for search functionality)
     * @param query Query fingerprint to search for
     * @param similarityScore Output parameter for similarity score (0.0-1.0)
     * @return True if query is found within this fingerprint
     */
    bool contains(const AudioFingerprint& query, double& similarityScore) const;
    
    /**
     * Calculate perceptual similarity between fingerprints
     * @param other Other fingerprint to compare with
     * @return Similarity score (0.0-1.0, higher is more similar)
     */
    double calculateSimilarity(const AudioFingerprint& other) const;
    
    // Serialization
    std::vector<uint8_t> serialize() const;

    /**
     * Deserialize an AudioFingerprint object from a byte array.
     * @param data Byte array containing the serialized fingerprint
     * @return AudioFingerprint object
     */
    static AudioFingerprint deserialize(const std::vector<uint8_t>& data);
    
    int getBlockCount() const { return timeFrequencyBlocks.size(); }
    int getFrequencyBands() const { return FREQUENCY_BANDS; }
    int getOriginalSampleRate() const { return originalSampleRate; }
    int getOriginalDuration() const { return originalDuration; }
    
    void printFingerprint() const;

    /**
     * Get the spectral centroid of the audio fingerprint.
     * This is the "center of mass" of the spectrum, indicating 
     * where the most energy is concentrated.
     * @return Vector of spectral centroid values
     */
    std::vector<double> getSpectralCentroid() const;

    /**
     * Get the spectral rolloff of the audio fingerprint.
     * This is the frequency below which a specified percentage of the 
     * total spectral energy is contained.
     * @return Vector of spectral rolloff values
     */
    std::vector<double> getSpectralRolloff() const;
    
private:

    /**
     * Compute the Fast Fourier Transform (FFT) of a signal.
     * @param samples Input audio samples
     * @return Frequency spectrum as complex numbers
     */
    std::vector<std::complex<double>> computeFFT(const std::vector<double>& samples) const;
    
    /**
     * Compute the inverse Fast Fourier Transform (IFFT) of a frequency spectrum.
     * @param spectrum Input frequency spectrum as complex numbers
     * @return Time-domain signal as real numbers
     */
    std::vector<double> computeInverseFFT(const std::vector<std::complex<double>>& spectrum) const;
    
    /**
     * Map a frequency in Hz to the Mel scale.
     * The Mel scale is a perceptual scale of pitches which approximates the human ear's response to different frequencies.
     * @param frequency Frequency in Hz
     * @return Frequency in Mel
     */
    double melScale(double frequency) const;
    
    /**
     * Map a frequency in Mel to Hz.
     * The inverse Mel scale is used to convert frequencies from the Mel scale back to Hz.
     * @param mel Frequency in Mel
     * @return Frequency in Hz
     */
    double inverseMelScale(double mel) const;

    /**
     * Create a Mel filter bank. 
     * A Mel Filter bank is a collection of triangular filters
     * that are spaced according to the Mel scale, which approximates the human ear's
     * perception of sound. This filter bank is used to extract Mel-frequency cepstral
     * coefficients (MFCCs) from audio signals.
     * @param fftSize Size of the FFT
     * @param sampleRate Sample rate of the audio
     * @return Vector of Mel filter bank coefficients
     */
    std::vector<double> createMelFilterBank(int fftSize, int sampleRate) const;
    
    /**
     * Quantize energy value to uint8_t, an 8-bit range with log scaling
     * Clamps the input to the valid range (0, 10)
     * Applies a gain factor to the output (Scales <gain> dB 0-255)
     * @param energy Energy value in dB
     * @return Quantized energy value
     */
    uint8_t quantizeEnergy(double energy) const;

    /**
     * Dequantize energy value from uint8_t to double
     * Maps the 8-bit range [0, 255] back to the original energy scale
     * Applies the inverse of the gain factor used in quantization
     * Clamps the output to the valid range (0, 10)
     * @param quantized Quantized energy value
     * @return Dequantized energy value
     */
    double dequantizeEnergy(uint8_t quantized) const;
    
    /**
     * Compute correlation between two time-frequency windows
     * @param window1 First window (reference)
     * @param window2 Second window (query)
     * @return Correlation score (0.0-1.0)
     */
    double correlateWindows(const std::vector<std::vector<uint8_t>>& window1, const std::vector<std::vector<uint8_t>>& window2) const;
};

} // namespace AudioBabel

/* Known issues / suggested fixes:
 *  - `fromSamples` computes `numBlocks = (samples.size() - blockSamples) / hopSize + 1` which
 *    can be negative or overflow for very short inputs. Add guards/padding for short audio.
 *  - FFTW plans are created/destroyed per-block (high overhead). Reuse plans or use
 *    batch FFTs to improve performance.
 *  - `toSamples` uses std::rand() for random phase; this is non-deterministic and
 *    not thread-safe. Use a seeded std::mt19937 for reproducible reconstruction.
 *  - Serialization/deserialization assumes exact sizes; add sanity checks to avoid
 *    buffer-overrun when reading corrupted data.
 */

#endif // AUDIO_FINGERPRINT_H
