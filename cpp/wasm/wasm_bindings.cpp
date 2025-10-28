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
#include "../include/LibraryPosition.h"
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
 * Get the size of audio data for a given duration
 */
EMSCRIPTEN_KEEPALIVE
int calculateAudioSize(int durationSeconds, int sampleRate, int bitDepth, int channels) {
    int samples = durationSeconds * sampleRate * channels;
    int bytes   = samples * (bitDepth / 8);
    return bytes;
}

} // extern "C"

// Wrapper functions that return std::string for embind compatibility
std::string getMetadataWrapper(const std::string& base64Index) {
    char*       result = getMetadataFromBase64(base64Index.c_str());
    std::string str(result);
    free(result);
    return str;
}

std::string generateRandomWrapper(int targetLength) {
    char*       result = generateRandomIndex(targetLength);
    std::string str(result);
    free(result);
    return str;
}

// Wrapper for reconstructAudioFromBase64 that returns a JavaScript-compatible typed array
emscripten::val reconstructAudioWrapper(const std::string& base64Index) {
    try {
        int      length = 0;
        uint8_t* data   = reconstructAudioFromBase64(base64Index.c_str(), &length);

        if (data == nullptr || length == 0) {
            return emscripten::val::null();
        }

        // Create a Uint8Array from the data
        emscripten::val result = emscripten::val(emscripten::typed_memory_view(length, data));

        // Copy the data to JavaScript heap (so we can free the C++ memory)
        emscripten::val copy = emscripten::val::global("Uint8Array").new_(result);

        // Free the C++ memory
        free(data);

        return copy;
    } catch (const std::exception& e) {
        std::cerr << "Error in reconstructAudioWrapper: " << e.what() << std::endl;
        return emscripten::val::null();
    }
}

// Embind bindings for class-based API
using namespace emscripten;

EMSCRIPTEN_BINDINGS(audio_index_module) {
    // Expose utility functions (using std::string wrappers)
    function("getMetadata", &getMetadataWrapper);
    function("generateRandom", &generateRandomWrapper);
    function("reconstructAudio", &reconstructAudioWrapper);

    // Functions that don't need wrappers
    function("calculateSize", &calculateAudioSize);
}
