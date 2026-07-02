// Emscripten embind wrappers exposing the AudioBabel library to JavaScript.
// See cpp/wasm/README.md for the exported function list and
// docs/js/core/indexWasm.js for the JS-side integration.

#include <emscripten/bind.h>
#include <emscripten/emscripten.h>
#include <emscripten/val.h>

#include <algorithm>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "../include/Index.h"
#include "../include/IndexMetadata.h"
#include "../include/IndexNaming.h"
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

// Render a single `"key":"<escaped value>"` JSON field.
static std::string jsonStringField(const std::string& key, const std::string& value) {
    return "\"" + key + "\":\"" + escapeJsonString(value) + "\"";
}

// Render a single `"key":<number>` JSON field.
static std::string jsonNumberField(const std::string& key, long long value) {
    return "\"" + key + "\":" + std::to_string(value);
}

// Render a JSON array of strings: ["a","b",...]
static std::string jsonStringArray(const std::vector<std::string>& values) {
    std::string json = "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0)
            json += ",";
        json += "\"" + escapeJsonString(values[i]) + "\"";
    }
    json += "]";
    return json;
}

// Direct embind-compatible functions returning std::string / emscripten::val.

// Returns JSON: {"genre":"...", "artist":"...", "album":"...", "track":"...", "cover":"..."}
static std::string getMetadataWrapper(const std::string& base64Index) {
    try {
        IndexMetadata metadata = IndexMetadata::extractMetadataFromIndex(base64Index);

        std::string json = "{";
        json += jsonStringField("genre", metadata.genre) + ",";
        json += jsonStringField("artist", metadata.artist) + ",";
        json += jsonStringField("album", metadata.album) + ",";
        json += jsonStringField("track", metadata.track) + ",";
        json += jsonStringField("cover", metadata.cover);
        json += "}";
        return json;

    } catch (const std::exception& e) {
        return makeJsonError(e.what());
    }
}

// Decodes a base64 index in one pass: b64ToIndex runs once, then metadata,
// position, and PCM are all derived from the same cpp_int. Returns a JS
// object {metadataJson, positionJson, pcm: Uint8Array}, or null on error.
static emscripten::val decodeIndexWrapper(const std::string& base64Index) {
    try {
        cpp_int index = AudioBabel::Utilities::b64ToIndex(base64Index);

        // Metadata + position in one call (calculateLibraryPosition runs once inside).
        IndexMetadata meta = IndexMetadata::extractMetadataFromIndex(index);

        std::string metaJson = "{";
        metaJson += jsonStringField("genre", meta.genre) + ",";
        metaJson += jsonStringField("artist", meta.artist) + ",";
        metaJson += jsonStringField("album", meta.album) + ",";
        metaJson += jsonStringField("track", meta.track) + ",";
        metaJson += jsonStringField("cover", meta.cover);
        metaJson += "}";

        const LibraryPosition& pos     = meta.position;
        std::string            posJson = "{";
        posJson += jsonStringField("room", pos.room) + ",";
        posJson += jsonNumberField("wall", pos.wall) + ",";
        posJson += jsonNumberField("shelf", pos.shelf) + ",";
        posJson += jsonNumberField("album", pos.album) + ",";
        posJson += jsonNumberField("track", pos.track);
        posJson += "}";

        // PCM — Index::decode runs unscramble internally.
        std::vector<uint8_t> samples = Index::decode(index);
        emscripten::val      pcmView = emscripten::val(emscripten::typed_memory_view(samples.size(), samples.data()));
        emscripten::val      pcm     = emscripten::val::global("Uint8Array").new_(pcmView);

        emscripten::val result = emscripten::val::object();
        result.set("metadataJson", emscripten::val(metaJson));
        result.set("positionJson", emscripten::val(posJson));
        result.set("pcm", pcm);
        return result;

    } catch (const std::exception& e) {
        std::cerr << "[decodeIndexWrapper] Exception: " << e.what() << std::endl;
        return emscripten::val::null();
    }
}

static std::string reconstructIndexWrapper(const std::string& roomStr, int wall, int shelf, int album, int track) {
    try {
        LibraryPosition pos;
        pos.room  = roomStr;
        pos.wall  = static_cast<uint8_t>(wall);
        pos.shelf = static_cast<uint8_t>(shelf);
        pos.album = static_cast<uint8_t>(album);
        pos.track = static_cast<uint8_t>(track);

        cpp_int index = AudioBabel::reconstructIndexFromPosition(pos);

        return AudioBabel::Utilities::indexToB64(index);

    } catch (const std::exception& e) {
        return makeJsonError(e.what());
    }
}

// Forward direction (PCM -> index); decodeIndex is its inverse.
static std::string encodeIndexWrapper(const emscripten::val& pcmBytes, int sampleRate, int bitDepth, int numChannels) {
    // sampleRate/bitDepth/numChannels are accepted for API compatibility with callers,
    // but the bijection is payload-only and never uses them.
    (void) sampleRate;
    (void) bitDepth;
    (void) numChannels;
    try {
        // convertJSArrayToNumberVector bulk-copies a JS typed array through the
        // WASM heap in one shot; vecFromJSArray marshals element-by-element via
        // val, which is dramatically slower for multi-MB PCM uploads.
        std::vector<uint8_t> samples = emscripten::convertJSArrayToNumberVector<uint8_t>(pcmBytes);

        cpp_int index = Index::encode(samples);
        return AudioBabel::Utilities::indexToB64(index);

    } catch (const std::exception& e) {
        return makeJsonError(e.what());
    }
}

// Return library hierarchy constants as JSON so JS doesn't need to hardcode them.
static std::string getLibraryConstantsWrapper() {
    std::string json = "{";
    json += jsonNumberField("tracksPerAlbum", LibraryConstants::TRACKS_PER_ALBUM) + ",";
    json += jsonNumberField("albumsPerShelf", LibraryConstants::ALBUMS_PER_SHELF) + ",";
    json += jsonNumberField("shelvesPerWall", LibraryConstants::SHELVES_PER_WALL) + ",";
    json += jsonNumberField("wallsPerRoom", LibraryConstants::WALLS_PER_ROOM) + ",";
    json += jsonNumberField("nameMaxChars", static_cast<long long>(IndexNaming::nameMaxChars())) + ",";
    // Cover mosaic dimensions, so the JS cover-art search can quantize an
    // uploaded image to exactly the pixel grid constructByCover expects.
    json += jsonNumberField("coverPixelsPerSide", static_cast<long long>(IndexMetadata::CANVAS_SIZE / IndexMetadata::DEFAULT_CELL_SIZE)) + ",";
    json += jsonNumberField("coverPixelBytes", static_cast<long long>(IndexMetadata::pixelBytesNeeded(IndexMetadata::DEFAULT_CELL_SIZE)));
    json += "}";
    return json;
}

// Batch name generators for the browse UI — one call per rendered level
// instead of one getMetadata() call per sibling. Each returns a JSON array
// of names ordered by slot, or a JSON error object on invalid input.
static std::string getGenreNamesWrapper(const std::string& roomStr) {
    try {
        return jsonStringArray(IndexNaming::genreNames(roomStr));
    } catch (const std::exception& e) {
        return makeJsonError(e.what());
    }
}

static std::string getArtistNamesWrapper(const std::string& roomStr, int wall) {
    try {
        return jsonStringArray(IndexNaming::artistNames(roomStr, static_cast<uint8_t>(wall)));
    } catch (const std::exception& e) {
        return makeJsonError(e.what());
    }
}

static std::string getAlbumNamesWrapper(const std::string& roomStr, int wall, int shelf) {
    try {
        return jsonStringArray(IndexNaming::albumNames(roomStr, static_cast<uint8_t>(wall), static_cast<uint8_t>(shelf)));
    } catch (const std::exception& e) {
        return makeJsonError(e.what());
    }
}

static std::string getTrackNamesWrapper(const std::string& roomStr, int wall, int shelf, int album) {
    try {
        return jsonStringArray(
            IndexNaming::trackNames(roomStr, static_cast<uint8_t>(wall), static_cast<uint8_t>(shelf), static_cast<uint8_t>(album)));
    } catch (const std::exception& e) {
        return makeJsonError(e.what());
    }
}

// Serializes constructed candidate indexes into the JSON result array shared
// by constructByNames and constructByCover: each entry carries the index
// itself, its library position, and the four metadata names it decodes to.
static std::string constructedIndexesToJson(const std::vector<cpp_int>& indexes) {
    std::string json = "[";
    for (size_t i = 0; i < indexes.size(); ++i) {
        if (i > 0) {
            json += ",";
        }
        const cpp_int&     idx   = indexes[i];
        LibraryPosition    pos   = AudioBabel::calculateLibraryPosition(idx);
        IndexNaming::Names names = IndexNaming::namesForIndex(idx);

        json += "{";
        json += jsonStringField("indexBase64", AudioBabel::Utilities::indexToB64(idx)) + ",";
        json += jsonStringField("room", pos.room) + ",";
        json += jsonNumberField("wall", pos.wall) + ",";
        json += jsonNumberField("shelf", pos.shelf) + ",";
        json += jsonNumberField("album", pos.album) + ",";
        json += jsonNumberField("track", pos.track) + ",";
        json += jsonStringField("genreName", names.genre) + ",";
        json += jsonStringField("artistName", names.artist) + ",";
        json += jsonStringField("albumName", names.album) + ",";
        json += jsonStringField("trackName", names.track);
        json += "}";
    }
    json += "]";
    return json;
}

// Constructs indexes that carry the requested metadata names (see
// IndexNaming.h). Each of genre/artist/album/track is either a name to pin
// down or an empty string meaning "leave free" (randomized per result, seeded
// by `seed`). Returns the JSON array constructedIndexesToJson produces, or a
// JSON error object.
static std::string constructByNamesWrapper(
    const std::string& genre, const std::string& artist, const std::string& album, const std::string& track, int maxResults, double seed) {
    try {
        IndexNaming::NameQuery query;
        if (!genre.empty()) {
            query.genre = genre;
        }
        if (!artist.empty()) {
            query.artist = artist;
        }
        if (!album.empty()) {
            query.album = album;
        }
        if (!track.empty()) {
            query.track = track;
        }

        size_t   count = static_cast<size_t>(std::max(0, maxResults));
        uint64_t s     = static_cast<uint64_t>(seed);

        std::vector<cpp_int> indexes = IndexNaming::constructIndexesForNames(query, count, s);
        return constructedIndexesToJson(indexes);

    } catch (const std::exception& e) {
        return makeJsonError(e.what());
    }
}

// Constructs indexes whose cover art renders the given pixel bytes (see
// IndexMetadata::constructIndexesForCover). `pixelBytes` must be a Uint8Array
// of exactly coverPixelBytes (see getLibraryConstants) packed 8-bit RGB values
// in reading order — the caller quantizes the uploaded image to the
// coverPixelsPerSide grid first. Returns the same JSON shape as
// constructByNames, or a JSON error object.
static std::string constructByCoverWrapper(const emscripten::val& pixelBytes, int maxResults, double seed) {
    try {
        std::vector<uint8_t> pixels = emscripten::convertJSArrayToNumberVector<uint8_t>(pixelBytes);

        size_t   count = static_cast<size_t>(std::max(0, maxResults));
        uint64_t s     = static_cast<uint64_t>(seed);

        std::vector<cpp_int> indexes = IndexMetadata::constructIndexesForCover(pixels, count, s);
        return constructedIndexesToJson(indexes);

    } catch (const std::exception& e) {
        return makeJsonError(e.what());
    }
}

// Embind bindings for class-based API
using namespace emscripten;

EMSCRIPTEN_BINDINGS(audio_index_module) {
    // Expose utility functions (using std::string wrappers)
    function("getMetadata", &getMetadataWrapper);
    function("decodeIndex", &decodeIndexWrapper);
    function("reconstructIndex", &reconstructIndexWrapper);
    function("encodeIndex", &encodeIndexWrapper);

    // Library hierarchy constants (R5 — avoids manual duplication in JS)
    function("getLibraryConstants", &getLibraryConstantsWrapper);

    // Batch cosmetic-name generators for the browse UI
    function("getGenreNames", &getGenreNamesWrapper);
    function("getArtistNames", &getArtistNamesWrapper);
    function("getAlbumNames", &getAlbumNamesWrapper);
    function("getTrackNames", &getTrackNamesWrapper);

    // Construct indexes from metadata names (see IndexNaming.h)
    function("constructByNames", &constructByNamesWrapper);

    // Construct indexes from target cover-art pixels (see IndexMetadata.h)
    function("constructByCover", &constructByCoverWrapper);
}
