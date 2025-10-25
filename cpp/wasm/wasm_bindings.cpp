/**
 * WebAssembly bindings for Audio Index library
 * Exposes C++ functions to JavaScript
 */

#include <emscripten/bind.h>
#include <emscripten/emscripten.h>

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "../include/AudioIndex.h"
#include "../include/IndexMetadata.h"
#include "../include/Utilities.h"

using namespace AudioBabel;
using boost::multiprecision::cpp_int;

// External C functions for JavaScript interop
extern "C" {

/**
 * Convert base64 string to index and reconstruct audio metadata
 * Returns JSON string with position and metadata
 */
EMSCRIPTEN_KEEPALIVE
char* getMetadataFromBase64(const char* base64Index) {
    try {
        std::string indexStr(base64Index);

        // Parse index to get metadata
        IndexMetadata metadata = IndexMetadata::extractMetadataFromIndex(indexStr);

        // Build JSON response (simple manual JSON construction)
        std::string json = "{";
        json += "\"genre\":\"" + metadata.genre + "\",";
        json += "\"artist\":\"" + metadata.artist + "\",";
        json += "\"album\":\"" + metadata.album + "\",";
        json += "\"track\":\"" + metadata.track + "\",";
        json += "\"cover\":\"" + metadata.cover + "\"";
        json += "}";

        // Allocate string on heap for JavaScript
        char* result = (char*) malloc(json.length() + 1);
        strcpy(result, json.c_str());
        return result;

    } catch (const std::exception& e) {
        std::string error  = "{\"error\":\"" + std::string(e.what()) + "\"}";
        char*       result = (char*) malloc(error.length() + 1);
        strcpy(result, error.c_str());
        return result;
    }
}

/**
 * Reconstruct audio samples from base64 index
 * Returns pointer to audio data structure (passed to JS as typed array)
 */
EMSCRIPTEN_KEEPALIVE
uint8_t* reconstructAudioFromBase64(const char* base64Index, int* outLength) {
    try {
        std::string indexStr(base64Index);

        // Decode base64 to get index
        std::vector<uint8_t> indexBytes = AudioBabel::Utilities::decodeBase64Url(indexStr);

        // Convert bytes to cpp_int
        cpp_int index = 0;
        boost::multiprecision::import_bits(index, indexBytes.begin(), indexBytes.end(), 8, true);

        // Reconstruct audio data
        AudioIndex::AudioData audioData = AudioIndex::indexToAudioData(index);

        // Return sample data
        *outLength      = static_cast<int>(audioData.samples.size());
        uint8_t* result = (uint8_t*) malloc(*outLength);
        memcpy(result, audioData.samples.data(), *outLength);

        return result;

    } catch (const std::exception& e) {
        *outLength = 0;
        return nullptr;
    }
}

/**
 * Generate index from audio samples
 * Takes sample data and returns base64 index
 */
EMSCRIPTEN_KEEPALIVE
char* generateIndexFromSamples(const uint8_t* samples, int sampleCount, int sampleRate, int bitDepth) {
    try {
        // Convert samples to vector
        std::vector<int32_t> sampleVec;
        int                  bytesPerSample = bitDepth / 8;

        for (int i = 0; i < sampleCount / bytesPerSample; i++) {
            int32_t sample = 0;
            for (int j = 0; j < bytesPerSample; j++) {
                sample |= static_cast<int32_t>(samples[i * bytesPerSample + j]) << (j * 8);
            }
            sampleVec.push_back(sample);
        }

        // Create AudioIndex from samples
        AudioIndex index = AudioIndex::fromAudioSamples(sampleVec, sampleRate, bitDepth);

        // Convert to base64 (we need to add this functionality)
        // For now, return placeholder
        std::string base64 = "generated_index";

        char* result = (char*) malloc(base64.length() + 1);
        strcpy(result, base64.c_str());
        return result;

    } catch (const std::exception& e) {
        std::string error  = "error:" + std::string(e.what());
        char*       result = (char*) malloc(error.length() + 1);
        strcpy(result, error.c_str());
        return result;
    }
}

/**
 * Generate a random valid index for Library of Babel exploration
 */
EMSCRIPTEN_KEEPALIVE
char* generateRandomIndex(int targetLength) {
    try {
        // URL-safe base64 alphabet
        const char alphabet[]   = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        const int  alphabetSize = 64;

        std::string randomIndex;
        randomIndex.reserve(targetLength);

        // Generate random base64 string
        for (int i = 0; i < targetLength; i++) {
            int randIdx = rand() % alphabetSize;
            randomIndex += alphabet[randIdx];
        }

        char* result = (char*) malloc(randomIndex.length() + 1);
        strcpy(result, randomIndex.c_str());
        return result;

    } catch (const std::exception& e) {
        char* result = (char*) malloc(6);
        strcpy(result, "error");
        return result;
    }
}

/**
 * Validate a base64 index string
 */
EMSCRIPTEN_KEEPALIVE
int validateBase64Index(const char* base64Index) {
    try {
        std::string indexStr(base64Index);
        return AudioBabel::Utilities::isValidBase64Url(indexStr) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

/**
 * Get the size of audio data for a given duration
 */
EMSCRIPTEN_KEEPALIVE
int calculateAudioSize(int durationSeconds, int sampleRate, int bitDepth, int channels) {
    int samples = durationSeconds * sampleRate * channels;
    int bytes   = samples * (bitDepth / 8);
    return bytes;
}

/**
 * Convert 16-bit audio samples to sample-based base64 (3 chars per sample)
 * Returns base64 string
 */
EMSCRIPTEN_KEEPALIVE
char* audioSamplesToSampleBase64(const uint8_t* samples, int sampleCount, int sampleRate, int channels) {
    try {
        // Create AudioData structure
        AudioIndex::AudioData audioData{};
        audioData.sample_rate  = static_cast<uint32_t>(sampleRate);
        audioData.bit_rate     = 16; // Only 16-bit supported
        audioData.num_channels = static_cast<uint16_t>(channels);
        audioData.audio_format = 1; // PCM
        audioData.num_frames   = sampleCount / channels;

        // Copy samples
        audioData.samples.resize(sampleCount * 2); // 16-bit = 2 bytes per sample
        memcpy(audioData.samples.data(), samples, sampleCount * 2);

        // Encode to sample-based base64
        std::string base64 = AudioIndex::audioDataToSampleBase64(audioData);

        char* result = (char*) malloc(base64.length() + 1);
        strcpy(result, base64.c_str());
        return result;

    } catch (const std::exception& e) {
        std::string error  = "error:" + std::string(e.what());
        char*       result = (char*) malloc(error.length() + 1);
        strcpy(result, error.c_str());
        return result;
    }
}

/**
 * Convert sample-based base64 string back to audio samples
 * Returns audio sample data
 */
EMSCRIPTEN_KEEPALIVE
uint8_t* sampleBase64ToAudioSamples(const char* base64String, int sampleRate, int channels, int* outLength) {
    try {
        std::string base64Str(base64String);

        // Decode from sample-based base64
        AudioIndex::AudioData audioData =
            AudioIndex::sampleBase64ToAudioData(base64Str, static_cast<uint32_t>(sampleRate), static_cast<uint16_t>(channels));

        // Return sample data
        *outLength      = static_cast<int>(audioData.samples.size());
        uint8_t* result = (uint8_t*) malloc(*outLength);
        memcpy(result, audioData.samples.data(), *outLength);

        return result;

    } catch (const std::exception& e) {
        *outLength = 0;
        return nullptr;
    }
}

} // extern "C"

// Embind bindings for class-based API
using namespace emscripten;

EMSCRIPTEN_BINDINGS(audio_index_module) {
    // Expose utility functions
    function("getMetadata", &getMetadataFromBase64, allow_raw_pointers());
    function("reconstructAudio", &reconstructAudioFromBase64, allow_raw_pointers());
    function("generateIndex", &generateIndexFromSamples, allow_raw_pointers());
    function("generateRandom", &generateRandomIndex, allow_raw_pointers());
    function("validate", &validateBase64Index, allow_raw_pointers());
    function("calculateSize", &calculateAudioSize);

    // Sample-based base64 functions
    function("audioToSampleBase64", &audioSamplesToSampleBase64, allow_raw_pointers());
    function("sampleBase64ToAudio", &sampleBase64ToAudioSamples, allow_raw_pointers());
}
