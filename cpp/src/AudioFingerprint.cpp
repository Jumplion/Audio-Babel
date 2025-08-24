#include "AudioFingerprint.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <fftw3.h>
#include <iostream>
#include <cstring>

#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace AudioBabel {

/*
 * AudioFingerprint.cpp
 * --------------------
 * Purpose: Implements fingerprint creation from PCM audio, serialization,
 *          and simple spectral reconstruction. Uses FFTW for transforms.
 *
 * Performance notes / pitfalls:
 *  - FFTW plans are allocated/destroyed per FFT call; reuse plans where possible.
 *  - Use of std::rand() for phase makes reconstruction non-deterministic.
 *  - `fromSamples` should validate and pad very-short inputs to avoid negative
 *    `numBlocks` calculations.
 */
AudioFingerprint::AudioFingerprint() : originalSampleRate(48000), originalDuration(8) {
}

AudioFingerprint::AudioFingerprint(const AudioFingerprint& other) 
    : timeFrequencyBlocks(other.timeFrequencyBlocks),
        originalSampleRate(other.originalSampleRate),
        originalDuration(other.originalDuration) {
}

AudioFingerprint& AudioFingerprint::operator=(const AudioFingerprint& other) {
    if (this != &other) {
        timeFrequencyBlocks = other.timeFrequencyBlocks;
        originalSampleRate = other.originalSampleRate;
        originalDuration = other.originalDuration;
    }
    return *this;
}

AudioFingerprint::~AudioFingerprint() {
}

AudioFingerprint AudioFingerprint::fromSamples(const std::vector<int32_t>& samples, int sampleRate) {
    AudioFingerprint fingerprint;
    fingerprint.originalSampleRate = sampleRate;
    fingerprint.originalDuration = static_cast<int>(samples.size() / sampleRate);
    
    // Calculate block parameters
    int blockSamples = (BLOCK_SIZE_MS * sampleRate) / 1000;
    int hopSize = blockSamples * (100 - OVERLAP_PERCENT) / 100;
    int numBlocks = (samples.size() - blockSamples) / hopSize + 1;
    
    fingerprint.timeFrequencyBlocks.reserve(numBlocks);
    
    // Create mel filter bank
    std::vector<double> melFilters = fingerprint.createMelFilterBank(blockSamples, sampleRate);
    
    for (int block = 0; block < numBlocks; ++block) {
        int startSample = block * hopSize;
        int endSample = std::min(startSample + blockSamples, static_cast<int>(samples.size()));
        
        // Extract block samples and convert to double
        std::vector<double> blockData(blockSamples, 0.0);
        for (int i = 0; i < endSample - startSample; ++i) {
            blockData[i] = static_cast<double>(samples[startSample + i]) / INT32_MAX;
        }
        
        // Apply window function (Hamming window)
        for (int i = 0; i < blockSamples; ++i) {
            double window = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (blockSamples - 1));
            blockData[i] *= window;
        }
        
        // Compute FFT
        std::vector<std::complex<double>> spectrum = fingerprint.computeFFT(blockData);
        
        // Apply mel filter bank and quantize
        std::vector<uint8_t> energies(FREQUENCY_BANDS);
        for (int band = 0; band < FREQUENCY_BANDS; ++band) {
            double energy = 0.0;
            int startBin = (band * spectrum.size()) / (2 * FREQUENCY_BANDS);
            int endBin = ((band + 1) * spectrum.size()) / (2 * FREQUENCY_BANDS);
            
            for (int bin = startBin; bin < endBin && bin < spectrum.size() / 2; ++bin) {
                energy += std::norm(spectrum[bin]);
            }
            
            energy = std::log1p(energy); // Log compression
            energies[band] = fingerprint.quantizeEnergy(energy);
        }
        
        fingerprint.timeFrequencyBlocks.push_back(energies);
    }
    
    return fingerprint;
}

std::vector<int32_t> AudioFingerprint::toSamples(int sampleRate) const {
    if (timeFrequencyBlocks.empty()) {
        return std::vector<int32_t>();
    }
    
    int blockSamples = (BLOCK_SIZE_MS * sampleRate) / 1000;
    int hopSize = blockSamples * (100 - OVERLAP_PERCENT) / 100;
    int totalSamples = timeFrequencyBlocks.size() * hopSize + blockSamples;
    
    std::vector<double> output(totalSamples, 0.0);
    std::vector<double> window(totalSamples, 0.0);
    
    for (size_t block = 0; block < timeFrequencyBlocks.size(); ++block) {
        // Reconstruct spectrum from quantized energies
        std::vector<std::complex<double>> spectrum(blockSamples);
        
        for (int band = 0; band < FREQUENCY_BANDS; ++band) {
            double energy = dequantizeEnergy(timeFrequencyBlocks[block][band]);
            energy = std::expm1(energy); // Inverse log compression
            
            int startBin = (band * blockSamples) / (2 * FREQUENCY_BANDS);
            int endBin = ((band + 1) * blockSamples) / (2 * FREQUENCY_BANDS);
            
            for (int bin = startBin; bin < endBin && bin < blockSamples / 2; ++bin) {
                // Use random phase for reconstruction
                double phase = 2.0 * M_PI * (std::rand() / static_cast<double>(RAND_MAX));
                spectrum[bin] = std::polar(std::sqrt(energy), phase);
                
                // Mirror for real-valued signal
                if (bin > 0 && bin < blockSamples / 2) {
                    spectrum[blockSamples - bin] = std::conj(spectrum[bin]);
                }
            }
        }
        
        // Convert back to time domain
        std::vector<double> blockSamples_vec = computeInverseFFT(spectrum);
        
        // Apply window and overlap-add
        int startSample = block * hopSize;
        for (int i = 0; i < blockSamples && startSample + i < totalSamples; ++i) {
            double windowValue = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (blockSamples - 1));
            output[startSample + i] += blockSamples_vec[i] * windowValue;
            window[startSample + i] += windowValue * windowValue;
        }
    }
    
    // Normalize by window overlap
    for (size_t i = 0; i < output.size(); ++i) {
        if (window[i] > 0.0) {
            output[i] /= window[i];
        }
    }
    
    // Convert to int32_t
    std::vector<int32_t> result(output.size());
    for (size_t i = 0; i < output.size(); ++i) {
        output[i] = std::max(-1.0, std::min(1.0, output[i])); // Clamp
        result[i] = static_cast<int32_t>(output[i] * INT32_MAX);
    }
    
    return result;
}

void AudioFingerprint::extractCodes(mpz_t genreCode, mpz_t artistCode, mpz_t albumCode, mpz_t trackCode) const {
    if (timeFrequencyBlocks.empty()) {
        mpz_set_ui(genreCode, 0);
        mpz_set_ui(artistCode, 0);
        mpz_set_ui(albumCode, 0);
        mpz_set_ui(trackCode, 0);
        return;
    }
    
    // Divide the fingerprint into regions for each hierarchical level
    size_t totalBlocks = timeFrequencyBlocks.size();
    size_t genreBlocks = totalBlocks / 4;
    size_t artistBlocks = totalBlocks / 4;
    size_t albumBlocks = totalBlocks / 4;
    size_t trackBlocks = totalBlocks - genreBlocks - artistBlocks - albumBlocks;
    
    auto extractFromRegion = [this](size_t start, size_t count, mpz_t result) {
        mpz_set_ui(result, 0);
        mpz_t base, temp;
        mpz_init(base);
        mpz_init(temp);
        mpz_set_ui(base, 256);
        
        for (size_t block = start; block < start + count && block < timeFrequencyBlocks.size(); ++block) {
            for (int band = 0; band < FREQUENCY_BANDS; ++band) {
                mpz_mul(result, result, base);
                mpz_set_ui(temp, timeFrequencyBlocks[block][band]);
                mpz_add(result, result, temp);
            }
        }
        
        mpz_clear(base);
        mpz_clear(temp);
    };
    
    extractFromRegion(0, genreBlocks, genreCode);
    extractFromRegion(genreBlocks, artistBlocks, artistCode);
    extractFromRegion(genreBlocks + artistBlocks, albumBlocks, albumCode);
    extractFromRegion(genreBlocks + artistBlocks + albumBlocks, trackBlocks, trackCode);
}

bool AudioFingerprint::contains(const AudioFingerprint& query, double& similarityScore) const {
    if (query.timeFrequencyBlocks.empty() || timeFrequencyBlocks.empty()) {
        similarityScore = 0.0;
        return false;
    }
    
    size_t queryBlocks = query.timeFrequencyBlocks.size();
    size_t searchBlocks = timeFrequencyBlocks.size();
    
    if (queryBlocks > searchBlocks) {
        similarityScore = 0.0;
        return false;
    }
    
    double maxCorrelation = 0.0;
    
    // Sliding window search
    for (size_t offset = 0; offset <= searchBlocks - queryBlocks; ++offset) {
        std::vector<std::vector<uint8_t>> window(
            timeFrequencyBlocks.begin() + offset,
            timeFrequencyBlocks.begin() + offset + queryBlocks
        );
        
        double correlation = correlateWindows(window, query.timeFrequencyBlocks);
        maxCorrelation = std::max(maxCorrelation, correlation);
    }
    
    similarityScore = maxCorrelation;
    return maxCorrelation > 0.7; // Threshold for "contains"
}

double AudioFingerprint::calculateSimilarity(const AudioFingerprint& other) const {
    if (timeFrequencyBlocks.empty() || other.timeFrequencyBlocks.empty()) {
        return 0.0;
    }
    
    size_t minBlocks = std::min(timeFrequencyBlocks.size(), other.timeFrequencyBlocks.size());
    
    double totalSimilarity = 0.0;
    for (size_t block = 0; block < minBlocks; ++block) {
        double blockSimilarity = 0.0;
        for (int band = 0; band < FREQUENCY_BANDS; ++band) {
            int diff = static_cast<int>(timeFrequencyBlocks[block][band]) - 
                    static_cast<int>(other.timeFrequencyBlocks[block][band]);
            blockSimilarity += std::exp(-0.1 * diff * diff); // Gaussian similarity
        }
        totalSimilarity += blockSimilarity / FREQUENCY_BANDS;
    }
    
    return totalSimilarity / minBlocks;
}

std::vector<uint8_t> AudioFingerprint::serialize() const {
    std::vector<uint8_t> result;
    
    // Write header
    uint32_t numBlocks = static_cast<uint32_t>(timeFrequencyBlocks.size());
    result.insert(result.end(), reinterpret_cast<const uint8_t*>(&originalSampleRate), 
                reinterpret_cast<const uint8_t*>(&originalSampleRate) + sizeof(originalSampleRate));
    result.insert(result.end(), reinterpret_cast<const uint8_t*>(&originalDuration), 
                reinterpret_cast<const uint8_t*>(&originalDuration) + sizeof(originalDuration));
    result.insert(result.end(), reinterpret_cast<const uint8_t*>(&numBlocks), 
                reinterpret_cast<const uint8_t*>(&numBlocks) + sizeof(numBlocks));
    
    // Write block data
    for (const auto& block : timeFrequencyBlocks) {
        result.insert(result.end(), block.begin(), block.end());
    }
    
    return result;
}

AudioFingerprint AudioFingerprint::deserialize(const std::vector<uint8_t>& data) {
    AudioFingerprint fingerprint;
    
    if (data.size() < sizeof(int) * 3) {
        return fingerprint;
    }
    
    size_t offset = 0;
    
    // Read header
    std::memcpy(&fingerprint.originalSampleRate, data.data() + offset, sizeof(fingerprint.originalSampleRate));
    offset += sizeof(fingerprint.originalSampleRate);
    
    std::memcpy(&fingerprint.originalDuration, data.data() + offset, sizeof(fingerprint.originalDuration));
    offset += sizeof(fingerprint.originalDuration);
    
    uint32_t numBlocks;
    std::memcpy(&numBlocks, data.data() + offset, sizeof(numBlocks));
    offset += sizeof(numBlocks);
    
    // Read block data
    fingerprint.timeFrequencyBlocks.resize(numBlocks);
    for (uint32_t block = 0; block < numBlocks && offset + FREQUENCY_BANDS <= data.size(); ++block) {
        fingerprint.timeFrequencyBlocks[block].resize(FREQUENCY_BANDS);
        std::memcpy(fingerprint.timeFrequencyBlocks[block].data(), data.data() + offset, FREQUENCY_BANDS);
        offset += FREQUENCY_BANDS;
    }
    
    return fingerprint;
}

// Private helper methods implementation
std::vector<std::complex<double>> AudioFingerprint::computeFFT(const std::vector<double>& samples) const {
    int n = samples.size();
    std::vector<std::complex<double>> result(n);
    
    // Allocate FFTW arrays
    fftw_complex* in = fftw_alloc_complex(n);
    fftw_complex* out = fftw_alloc_complex(n);
    
    // Copy input data
    for (int i = 0; i < n; ++i) {
        in[i][0] = samples[i];
        in[i][1] = 0.0;
    }
    
    // Create plan and execute
    fftw_plan plan = fftw_plan_dft_1d(n, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(plan);
    
    // Copy output data
    for (int i = 0; i < n; ++i) {
        result[i] = std::complex<double>(out[i][0], out[i][1]);
    }
    
    // Cleanup
    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);
    
    return result;
}

std::vector<double> AudioFingerprint::computeInverseFFT(const std::vector<std::complex<double>>& spectrum) const {
    int n = spectrum.size();
    std::vector<double> result(n);
    
    // Allocate FFTW arrays
    fftw_complex* in = fftw_alloc_complex(n);
    fftw_complex* out = fftw_alloc_complex(n);
    
    // Copy input data
    for (int i = 0; i < n; ++i) {
        in[i][0] = spectrum[i].real();
        in[i][1] = spectrum[i].imag();
    }
    
    // Create plan and execute
    fftw_plan plan = fftw_plan_dft_1d(n, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(plan);
    
    // Copy output data and normalize
    for (int i = 0; i < n; ++i) {
        result[i] = out[i][0] / n;
    }
    
    // Cleanup
    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);
    
    return result;
}

uint8_t AudioFingerprint::quantizeEnergy(double energy) const {
    // Quantize energy to 8-bit range with logarithmic scaling
    energy = std::max(0.0, std::min(10.0, energy)); // Clamp to reasonable range
    return static_cast<uint8_t>(energy * 25.5); // Scale to 0-255
}

double AudioFingerprint::dequantizeEnergy(uint8_t quantized) const {
    return static_cast<double>(quantized) / 25.5;
}

double AudioFingerprint::correlateWindows(const std::vector<std::vector<uint8_t>>& window1, const std::vector<std::vector<uint8_t>>& window2) const {
    if (window1.size() != window2.size()) {
        return 0.0;
    }
    
    double correlation = 0.0;
    double norm1 = 0.0, norm2 = 0.0;
    
    for (size_t block = 0; block < window1.size(); ++block) {
        for (int band = 0; band < FREQUENCY_BANDS; ++band) {
            double val1 = static_cast<double>(window1[block][band]);
            double val2 = static_cast<double>(window2[block][band]);
            
            correlation += val1 * val2;
            norm1 += val1 * val1;
            norm2 += val2 * val2;
        }
    }
    
    if (norm1 > 0.0 && norm2 > 0.0) {
        return correlation / std::sqrt(norm1 * norm2);
    }
    
    return 0.0;
}

std::vector<double> AudioFingerprint::createMelFilterBank(int fftSize, int sampleRate) const {
    // Simplified mel filter bank creation
    std::vector<double> filters(fftSize / 2);
    
    for (int i = 0; i < fftSize / 2; ++i) {
        double frequency = static_cast<double>(i * sampleRate) / fftSize;
        double mel = melScale(frequency);
        filters[i] = mel;
    }
    
    return filters;
}

double AudioFingerprint::melScale(double frequency) const {
    return 2595.0 * std::log10(1.0 + frequency / 700.0);
}

double AudioFingerprint::inverseMelScale(double mel) const {
    return 700.0 * (std::pow(10.0, mel / 2595.0) - 1.0);
}

// Fallback implementations — replace with real computation if available.
std::vector<double> AudioFingerprint::getSpectralCentroid() const {
    // return empty or computed centroid values
    return {};
}

std::vector<double> AudioFingerprint::getSpectralRolloff() const {
    // return empty or computed rolloff values
    return {};
}

} // namespace AudioBabel
