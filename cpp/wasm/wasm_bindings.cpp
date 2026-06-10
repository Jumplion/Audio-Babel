/**
 * @file wasm_bindings.cpp
 * @brief WebAssembly bindings for Audio Index library
 * 
 * Exposes C++ functions to JavaScript for browser-based audio indexing.
 * Uses Emscripten's embind API to create JavaScript-callable wrappers
 * for the core AudioBabel library.
 * 
 * @section exported_functions Exported Functions
 * - getMetadata: Extract metadata from base64 index string
 * - reconstructAudio: Decode index to PCM samples
 * - calculatePosition: Calculate library position from index
 * - reconstructIndex: Reconstruct index from library position
 * - calculateSize: Get the size of audio data for a given duration
 * - getLibraryConstants: Return library hierarchy constants as JSON
 * 
 * @see docs/js/audioIndexWasm.js for JavaScript integration wrapper
 */

#include <emscripten/bind.h>
#include <emscripten/emscripten.h>

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
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

// Direct embind-compatible functions returning std::string / emscripten::val.

/**
 * Extract metadata from a base64-encoded audio index.
 * Returns JSON: {"genre":"...", "artist":"...", "album":"...", "track":"...", "cover":"..."}
 */
static std::string getMetadataWrapper(const std::string& base64Index) {
    try {
        IndexMetadata metadata = IndexMetadata::extractMetadataFromIndex(base64Index);

        std::string json = "{";
        json += "\"genre\":\"" + escapeJsonString(metadata.genre) + "\",";
        json += "\"artist\":\"" + escapeJsonString(metadata.artist) + "\",";
        json += "\"album\":\"" + escapeJsonString(metadata.album) + "\",";
        json += "\"track\":\"" + escapeJsonString(metadata.track) + "\",";
        json += "\"cover\":\"" + escapeJsonString(metadata.cover) + "\"";
        json += "}";
        return json;

    } catch (const std::exception& e) {
        return makeJsonError(e.what());
    }
}

/**
 * Reconstruct PCM audio samples from a base64-encoded index.
 * Returns a JavaScript Uint8Array, or null on error.
 */
static emscripten::val reconstructAudioWrapper(const std::string& base64Index) {
    try {
        std::vector<uint8_t> indexBytes = AudioBabel::Utilities::decodeBase64Url(base64Index);

        cpp_int index = 0;
        boost::multiprecision::import_bits(index, indexBytes.begin(), indexBytes.end(), 8, true);

        AudioIndex::AudioData audioData = AudioIndex::indexToAudioData(index);

        emscripten::val view = emscripten::val(emscripten::typed_memory_view(audioData.samples.size(), audioData.samples.data()));
        return emscripten::val::global("Uint8Array").new_(view);

    } catch (const std::exception& e) {
        std::cerr << "[reconstructAudioWrapper] Exception: " << e.what() << std::endl;
        return emscripten::val::null();
    }
}

/**
 * Calculate library position from a base64 index.
 * Returns JSON: {"room":"base64", "wall":N, "shelf":N, "album":N, "track":N}
 */
static std::string calculatePositionWrapper(const std::string& base64Index) {
    try {
        std::vector<uint8_t> indexBytes = AudioBabel::Utilities::decodeBase64Url(base64Index);

        cpp_int index = 0;
        if (!indexBytes.empty()) {
            boost::multiprecision::import_bits(index, indexBytes.begin(), indexBytes.end(), 8, true);
        }

        LibraryPosition pos = AudioBabel::calculateLibraryPosition(index);

        std::string json = "{";
        json += "\"room\":\"" + escapeJsonString(pos.room) + "\",";
        json += "\"wall\":" + std::to_string(pos.wall) + ",";
        json += "\"shelf\":" + std::to_string(pos.shelf) + ",";
        json += "\"album\":" + std::to_string(pos.album) + ",";
        json += "\"track\":" + std::to_string(pos.track);
        json += "}";
        return json;

    } catch (const std::exception& e) {
        return makeJsonError(e.what());
    }
}

/**
 * Reconstruct a base64 index from a library position (lossless).
 */
static std::string reconstructIndexWrapper(const std::string& roomStr, int wall, int shelf, int album, int track) {
    try {
        LibraryPosition pos;
        pos.room  = roomStr;
        pos.wall  = static_cast<uint8_t>(wall);
        pos.shelf = static_cast<uint8_t>(shelf);
        pos.album = static_cast<uint8_t>(album);
        pos.track = static_cast<uint8_t>(track);

        cpp_int index = AudioBabel::reconstructIndexFromPosition(pos);

        std::vector<uint8_t> indexBytes;
        if (index == 0) {
            indexBytes.push_back(0);
        } else {
            boost::multiprecision::export_bits(index, std::back_inserter(indexBytes), 8, true);
        }

        return AudioBabel::Utilities::encodeBase64Url(indexBytes);

    } catch (const std::exception& e) {
        return makeJsonError(e.what());
    }
}

/**
 * Get the size of audio data for a given duration.
 */
static int calculateAudioSize(int durationSeconds, int sampleRate, int bitDepth, int channels) {
    int samples = durationSeconds * sampleRate * channels;
    int bytes   = samples * (bitDepth / 8);
    return bytes;
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
    function("reconstructAudio", &reconstructAudioWrapper);
    function("calculatePosition", &calculatePositionWrapper);
    function("reconstructIndex", &reconstructIndexWrapper);

    // Functions that don't need wrappers
    function("calculateSize", &calculateAudioSize);

    // Library hierarchy constants (R5 — avoids manual duplication in JS)
    function("getLibraryConstants", &getLibraryConstantsWrapper);
}
