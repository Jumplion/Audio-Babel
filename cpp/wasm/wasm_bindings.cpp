/**
 * @file wasm_bindings.cpp
 * @brief WebAssembly bindings for Audio Index library
 * 
 * Exposes C++ functions to JavaScript for browser-based audio indexing.
 * Uses Emscripten's embind API to create JavaScript-callable wrappers
 * for the core AudioBabel library.
 * 
 * @section memory_management Memory Management
 * Functions returning char* allocate memory with malloc() that must be
 * freed by JavaScript using Module._free(). See docs/js/audioIndexWasm.js
 * for proper usage patterns.
 * 
 * @section exported_functions Exported Functions
 * - getMetadataFromBase64: Extract metadata from base64 index string
 * - reconstructAudioFromBase64: Decode index to PCM samples
 * - generateIndexFromSamples: Encode PCM samples to base64 index
 * - getPositionFromBase64: Calculate library position from index
 * 
 * @see docs/js/audioIndexWasm.js for JavaScript integration wrapper
 */

#include <emscripten/bind.h>
#include <emscripten/emscripten.h>

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "../include/AudioIndex.h"
#include "../include/IndexMetadata.h"
#include "../include/LibraryPosition.h"
#include "../include/Utilities.h"

using namespace AudioBabel;
using boost::multiprecision::cpp_int;

// Escape a string for safe embedding in a JSON string value.
// Handles backslash, double-quote, and control characters.
static std::string escapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(ch));
                    out += buf;
                } else {
                    out += ch;
                }
                break;
        }
    }
    return out;
}

// Build a standard JSON error response: {"error":"<escaped msg>"}
static std::string makeJsonError(const std::string& message) {
    return "{\"error\":\"" + escapeJsonString(message) + "\"}";
}

// External C functions for JavaScript interop
extern "C" {

/**
 * @brief Extract metadata from a base64-encoded audio index.
 * 
 * Decodes a URL-safe base64 string and extracts hierarchical metadata
 * (genre, artist, album, track) and SVG cover art. Returns results as
 * a JSON string.
 * 
 * @param base64Index URL-safe base64 string (no padding) representing the index
 * @return Heap-allocated JSON string with metadata fields (must be freed by caller)
 * 
 * @par Return Format
 * Success: {"genre":"...", "artist":"...", "album":"...", "track":"...", "cover":"..."}
 * Error: {"error":"error message"}
 * 
 * @warning Caller must free the returned pointer using Module._free() in JavaScript
 * 
 * @see IndexMetadata::extractMetadataFromIndex for the underlying C++ implementation
 */
EMSCRIPTEN_KEEPALIVE
char* getMetadataFromBase64(const char* base64Index) {
    try {
        std::string indexStr(base64Index);

        // Parse index to get metadata
        IndexMetadata metadata = IndexMetadata::extractMetadataFromIndex(indexStr);

        // Build JSON response with escaped string values
        std::string json = "{";
        json += "\"genre\":\"" + escapeJsonString(metadata.genre) + "\",";
        json += "\"artist\":\"" + escapeJsonString(metadata.artist) + "\",";
        json += "\"album\":\"" + escapeJsonString(metadata.album) + "\",";
        json += "\"track\":\"" + escapeJsonString(metadata.track) + "\",";
        json += "\"cover\":\"" + escapeJsonString(metadata.cover) + "\"";
        json += "}";

        // Allocate string on heap for JavaScript
        char* result = (char*) malloc(json.length() + 1);
        strcpy(result, json.c_str());
        return result;

    } catch (const std::exception& e) {
        std::string error  = makeJsonError(e.what());
        char*       result = (char*) malloc(error.length() + 1);
        strcpy(result, error.c_str());
        return result;
    }
}

/**
 * @brief Reconstruct PCM audio samples from a base64-encoded index.
 * 
 * Decodes a URL-safe base64 string back to bytes, converts to a big integer
 * index, and reconstructs the original audio sample data. Returns raw PCM
 * bytes suitable for WAV file construction.
 * 
 * @param base64Index URL-safe base64 string (no padding) representing the index
 * @param[out] outLength Pointer to receive the length of returned byte array
 * @return Heap-allocated byte array with PCM samples (must be freed by caller)
 *         Returns nullptr and sets outLength=0 on error
 * 
 * @par Output Format
 * Raw PCM bytes in little-endian per-sample format, matching the original
 * audio encoding (8, 16, or 32 bits per sample).
 * 
 * @warning Caller must free the returned pointer using Module._free() in JavaScript
 * 
 * @see AudioIndex::indexToAudioData for the underlying C++ reconstruction
 */
EMSCRIPTEN_KEEPALIVE
uint8_t* reconstructAudioFromBase64(const char* base64Index, int* outLength) {
    try {
        std::string indexStr(base64Index);
        std::cout << "[reconstructAudioFromBase64] Input string length: " << indexStr.length() << std::endl;

        // Decode base64 to get index
        std::vector<uint8_t> indexBytes = AudioBabel::Utilities::decodeBase64Url(indexStr);
        std::cout << "[reconstructAudioFromBase64] Decoded bytes: " << indexBytes.size() << std::endl;

        if (indexBytes.size() > 0) {
            std::cout << "[reconstructAudioFromBase64] First few bytes: ";
            for (size_t i = 0; i < std::min(size_t(20), indexBytes.size()); ++i) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (int) indexBytes[i] << " ";
            }
            std::cout << std::dec << std::endl;
        }

        // Convert bytes to cpp_int
        cpp_int index = 0;
        boost::multiprecision::import_bits(index, indexBytes.begin(), indexBytes.end(), 8, true);
        std::cout << "[reconstructAudioFromBase64] Converted to cpp_int" << std::endl;

        // Reconstruct audio data
        AudioIndex::AudioData audioData = AudioIndex::indexToAudioData(index);
        std::cout << "[reconstructAudioFromBase64] Successfully reconstructed " << audioData.samples.size() << " sample bytes" << std::endl;

        // Return sample data
        *outLength      = static_cast<int>(audioData.samples.size());
        uint8_t* result = (uint8_t*) malloc(*outLength);
        memcpy(result, audioData.samples.data(), *outLength);

        return result;

    } catch (const std::exception& e) {
        std::cerr << "[reconstructAudioFromBase64] EXCEPTION: " << e.what() << std::endl;
        *outLength = 0;
        return nullptr;
    }
}

/**
 * @brief Generate a base64-encoded index from raw PCM samples.
 * 
 * Converts raw PCM sample bytes to a deterministic big integer index and
 * encodes it as a URL-safe base64 string. This is the primary function for
 * creating indexes from user-recorded audio in the browser.
 * 
 * @param samples Raw PCM sample bytes (little-endian per-sample)
 * @param sampleCount Total number of bytes in the samples array
 * @param sampleRate Sample rate in Hz (e.g., 44100)
 * @param bitDepth Bits per sample (8, 16, or 32)
 * @return Heap-allocated URL-safe base64 string (must be freed by caller)
 *         Returns "error:<message>" string on failure
 * 
 * @par Input Format
 * - samples: Raw PCM bytes, little-endian per sample
 * - sampleCount: Total byte count (not frame count)
 * - Mono audio expected (num_channels = 1)
 * 
 * @warning Caller must free the returned pointer using Module._free() in JavaScript
 * 
 * @see AudioIndex::audioDataToIndex for the underlying C++ encoding
 */
EMSCRIPTEN_KEEPALIVE
char* generateIndexFromSamples(const uint8_t* samples, int sampleCount, int sampleRate, int bitDepth) {
    try {
        // Convert raw bytes to signed int32 samples with proper sign extension
        std::vector<int32_t> sampleVec;
        int                  bytesPerSample = bitDepth / 8;

        for (int i = 0; i < sampleCount / bytesPerSample; i++) {
            uint32_t raw = 0;
            for (int j = 0; j < bytesPerSample; j++) {
                raw |= static_cast<uint32_t>(samples[i * bytesPerSample + j]) << (j * 8);
            }
            // Sign-extend: if the top bit of the sample word is set, fill upper bits
            int32_t sample;
            if (bytesPerSample == 2) {
                sample = static_cast<int32_t>(static_cast<int16_t>(raw));
            } else if (bytesPerSample == 4) {
                sample = static_cast<int32_t>(raw);
            } else {
                // 8-bit PCM is unsigned with 128 bias
                sample = static_cast<int32_t>(raw);
            }
            sampleVec.push_back(sample);
        }

        // Convert samples to AudioData and encode to big integer index
        AudioIndex::AudioData audioData = AudioIndex::extractAudioDataFromSamples(sampleVec, sampleRate, bitDepth);
        cpp_int               idx       = AudioIndex::audioDataToIndex(audioData);

        // Export big integer to bytes, then encode as URL-safe base64
        std::vector<uint8_t> indexBytes;
        if (idx == 0) {
            indexBytes.push_back(0);
        } else {
            boost::multiprecision::export_bits(idx, std::back_inserter(indexBytes), 8, true);
        }
        std::string base64 = AudioBabel::Utilities::encodeBase64Url(indexBytes);

        char* result = (char*) malloc(base64.length() + 1);
        strcpy(result, base64.c_str());
        return result;

    } catch (const std::exception& e) {
        std::string error  = makeJsonError(e.what());
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
        std::string error  = makeJsonError(e.what());
        char*       result = (char*) malloc(error.length() + 1);
        strcpy(result, error.c_str());
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

/**
 * Calculate library position from a base64 index (LOSSLESS)
 * Takes a base64-encoded index and returns JSON with position information
 * Returns: {"room": "base64", "wall": 0-3, "shelf": 0-4, "album": 0-31, "track": 0-14}
 * 
 * This is a pure encoding operation - converts any index to its unique position
 */
EMSCRIPTEN_KEEPALIVE
char* calculatePositionFromIndex(const char* base64Index) {
    try {
        std::string indexStr(base64Index);

        // Decode base64 to get index bytes
        std::vector<uint8_t> indexBytes = AudioBabel::Utilities::decodeBase64Url(indexStr);

        // Convert bytes to cpp_int (big-endian)
        cpp_int index = 0;
        if (!indexBytes.empty()) {
            boost::multiprecision::import_bits(index, indexBytes.begin(), indexBytes.end(), 8, true);
        }

        // Calculate library position using C++ function
        LibraryPosition pos = AudioBabel::calculateLibraryPosition(index);

        // Build JSON response with escaped string values
        std::string json = "{";
        json += "\"room\":\"" + escapeJsonString(pos.room) + "\",";
        json += "\"wall\":" + std::to_string(pos.wall) + ",";
        json += "\"shelf\":" + std::to_string(pos.shelf) + ",";
        json += "\"album\":" + std::to_string(pos.album) + ",";
        json += "\"track\":" + std::to_string(pos.track);
        json += "}";

        char* result = (char*) malloc(json.length() + 1);
        strcpy(result, json.c_str());
        return result;

    } catch (const std::exception& e) {
        std::string error  = makeJsonError(e.what());
        char*       result = (char*) malloc(error.length() + 1);
        strcpy(result, error.c_str());
        return result;
    }
}

/**
 * Reconstruct a base64 index from a library position (LOSSLESS)
 * Takes room (base64 string), wall, shelf, album, track
 * Returns the EXACT original base64-encoded index
 * 
 * This is a pure decoding operation:
 * 1. Decode room (base64) back to room number
 * 2. Reconstruct the index using: room*9600 + wall*2400 + shelf*480 + album*15 + track
 * 3. Convert index to base64
 * 
 * Position ↔ Index is perfectly bijective (no information loss)
 */
EMSCRIPTEN_KEEPALIVE
char* reconstructIndexFromPosition(const char* roomStr, int wall, int shelf, int album, int track) {
    try {
        // Create LibraryPosition with room as base64 string
        LibraryPosition pos;
        pos.room  = std::string(roomStr);
        pos.wall  = static_cast<uint8_t>(wall);
        pos.shelf = static_cast<uint8_t>(shelf);
        pos.album = static_cast<uint8_t>(album);
        pos.track = static_cast<uint8_t>(track);

        // Reconstruct the original index (lossless)
        cpp_int index = AudioBabel::reconstructIndexFromPosition(pos);

        // Convert index to base64 (this is the EXACT original index)
        std::vector<uint8_t> indexBytes;
        if (index == 0) {
            indexBytes.push_back(0);
        } else {
            boost::multiprecision::export_bits(index, std::back_inserter(indexBytes), 8, true);
        }

        std::string base64 = AudioBabel::Utilities::encodeBase64Url(indexBytes);

        char* result = (char*) malloc(base64.length() + 1);
        strcpy(result, base64.c_str());
        return result;

    } catch (const std::exception& e) {
        std::string error  = makeJsonError(e.what());
        char*       result = (char*) malloc(error.length() + 1);
        strcpy(result, error.c_str());
        return result;
    }
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

std::string calculatePositionWrapper(const std::string& base64Index) {
    char*       result = calculatePositionFromIndex(base64Index.c_str());
    std::string str(result);
    free(result);
    return str;
}

std::string reconstructIndexWrapper(const std::string& roomStr, int wall, int shelf, int album, int track) {
    char*       result = reconstructIndexFromPosition(roomStr.c_str(), wall, shelf, album, track);
    std::string str(result);
    free(result);
    return str;
}

// Wrapper for reconstructAudioFromBase64 that returns a JavaScript-compatible typed array
emscripten::val reconstructAudioWrapper(const std::string& base64Index) {
    try {
        std::cout << "[reconstructAudioWrapper] Input length: " << base64Index.length() << " chars" << std::endl;

        int      length = 0;
        uint8_t* data   = reconstructAudioFromBase64(base64Index.c_str(), &length);

        if (data == nullptr || length == 0) {
            std::cerr << "[reconstructAudioWrapper] Returned null or zero length" << std::endl;
            return emscripten::val::null();
        }

        std::cout << "[reconstructAudioWrapper] Successfully reconstructed " << length << " bytes" << std::endl;

        // Create a Uint8Array from the data
        emscripten::val result = emscripten::val(emscripten::typed_memory_view(length, data));

        // Copy the data to JavaScript heap (so we can free the C++ memory)
        emscripten::val copy = emscripten::val::global("Uint8Array").new_(result);

        // Free the C++ memory
        free(data);

        return copy;
    } catch (const std::exception& e) {
        std::cerr << "[reconstructAudioWrapper] Exception: " << e.what() << std::endl;
        return emscripten::val::null();
    }
}

// Return library hierarchy constants as JSON so JS doesn't need to hardcode them.
static std::string getLibraryConstantsWrapper() {
    return "{\"tracksPerAlbum\":" + std::to_string(LibraryConstants::TRACKS_PER_ALBUM) +
           ",\"albumsPerShelf\":" + std::to_string(LibraryConstants::ALBUMS_PER_SHELF) +
           ",\"shelvesPerWall\":" + std::to_string(LibraryConstants::SHELVES_PER_WALL) +
           ",\"wallsPerRoom\":" + std::to_string(LibraryConstants::WALLS_PER_ROOM) + "}";
}

// Embind bindings for class-based API
using namespace emscripten;

EMSCRIPTEN_BINDINGS(audio_index_module) {
    // Expose utility functions (using std::string wrappers)
    function("getMetadata", &getMetadataWrapper);
    function("generateRandom", &generateRandomWrapper);
    function("reconstructAudio", &reconstructAudioWrapper);
    function("calculatePosition", &calculatePositionWrapper);
    function("reconstructIndex", &reconstructIndexWrapper);

    // Functions that don't need wrappers
    function("calculateSize", &calculateAudioSize);

    // Library hierarchy constants (R5 — avoids manual duplication in JS)
    function("getLibraryConstants", &getLibraryConstantsWrapper);
}
