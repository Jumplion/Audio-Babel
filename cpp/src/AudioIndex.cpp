#include "AudioIndex.h"
#include "AudioFingerprint.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>
#include <algorithm>

namespace AudioBabel {
// AudioIndex.cpp
//
// Reorganized implementation for readability. Sections:
//  1) Binary IO helpers
//  2) Construction / lifecycle
//  3) Factory functions (from audio / hierarchy)
//  4) Serialization (serialize/deserialize)
//  5) Converters / accessors (toAudioSamples, get*String)
//  6) mpz <-> string helpers (base-94)
//  7) Browsing helpers (commented out)
//  8) Comparison operators
// ===========================================================================

// ---------------------------------------------------------------------------
// 1) Binary IO helpers
// ---------------------------------------------------------------------------
static void write_u64_le(std::ostream& out, uint64_t v) {
    uint8_t buf[8];
    for (int i = 0; i < 8; ++i) buf[i] = static_cast<uint8_t>((v >> (i * 8)) & 0xFF);
    out.write(reinterpret_cast<const char*>(buf), 8);
}

static bool read_u64_le(std::istream& in, uint64_t& v) {
    uint8_t buf[8];
    if (!in.read(reinterpret_cast<char*>(buf), 8)) return false;
    v = 0;
    for (int i = 0; i < 8; ++i) v |= (static_cast<uint64_t>(buf[i]) << (i * 8));
    return true;
}

// ---------------------------------------------------------------------------
// 2) Construction / lifecycle
// ---------------------------------------------------------------------------
AudioIndex::AudioIndex() : sampleRate(44100), duration(0.0), bitDepth(16) {
    initializeMpzValues();
}

AudioIndex::AudioIndex(const AudioIndex& other) : sampleRate(other.sampleRate), duration(other.duration), bitDepth(other.bitDepth), audioFingerprint(other.audioFingerprint) {
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

// ---------------------------------------------------------------------------
// 3) Factory functions
//    - fromAudioSamples: produce an AudioIndex from PCM samples
//    - fromHierarchy: deterministically build an AudioIndex from strings
// ---------------------------------------------------------------------------
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

    // Convert strings into mpz codes (base-94 encoding). Invalid strings map to zero.
    if (!index.stringToMpz(genreStr, index.genreCode)) mpz_set_ui(index.genreCode, 0);
    if (!index.stringToMpz(artistStr, index.artistCode)) mpz_set_ui(index.artistCode, 0);
    if (!index.stringToMpz(albumStr, index.albumCode)) mpz_set_ui(index.albumCode, 0);
    if (!index.stringToMpz(trackStr, index.trackCode)) mpz_set_ui(index.trackCode, 0);

    // Combine the textual base-94 digits for each component into a deterministic
    // payload that matches the serialized AudioFingerprint layout so that
    // AudioFingerprint::deserialize() can reconstruct a fingerprint deterministically.
    std::vector<uint8_t> combinedData;

    auto appendMpz = [&combinedData, &index](const mpz_t value) {
        std::string s = index.mpzToString(value);
        combinedData.insert(combinedData.end(), s.begin(), s.end());
    };

    appendMpz(index.genreCode);
    appendMpz(index.artistCode);
    appendMpz(index.albumCode);
    appendMpz(index.trackCode);

    // AudioFingerprint block layout expectations (must match AudioFingerprint impl)
    const size_t FREQUENCY_BANDS = 32;
    size_t numBlocks = (combinedData.size() + FREQUENCY_BANDS - 1) / FREQUENCY_BANDS;
    if (numBlocks == 0) numBlocks = 1;

    // Pad to full blocks
    combinedData.resize(numBlocks * FREQUENCY_BANDS, 0);

    // Build serialized fingerprint: originalSampleRate (int), originalDuration (int), numBlocks (uint32_t), blocks...
    std::vector<uint8_t> serialized;
    int originalSampleRate = index.sampleRate;
    serialized.insert(serialized.end(), reinterpret_cast<uint8_t*>(&originalSampleRate), reinterpret_cast<uint8_t*>(&originalSampleRate) + sizeof(originalSampleRate));
    int originalDuration = 0; // unknown
    serialized.insert(serialized.end(), reinterpret_cast<uint8_t*>(&originalDuration), reinterpret_cast<uint8_t*>(&originalDuration) + sizeof(originalDuration));
    uint32_t nb = static_cast<uint32_t>(numBlocks);
    serialized.insert(serialized.end(), reinterpret_cast<uint8_t*>(&nb), reinterpret_cast<uint8_t*>(&nb) + sizeof(nb));
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

// ---------------------------------------------------------------------------
// 4) Serialization
// ---------------------------------------------------------------------------
void AudioIndex::serialize(std::ostream& out) const {
    // Write basic properties
    out.write(reinterpret_cast<const char*>(&sampleRate), sizeof(sampleRate));
    out.write(reinterpret_cast<const char*>(&duration), sizeof(duration));
    out.write(reinterpret_cast<const char*>(&bitDepth), sizeof(bitDepth));

    // Helper: export an mpz_t as raw bytes, write length (u64 LE) then payload
    auto writeMpz = [&out](const mpz_t value) {
        size_t bits = mpz_sizeinbase(value, 2);
        size_t approxBytes = (bits + 7) / 8;
        std::vector<uint8_t> buffer(approxBytes > 0 ? approxBytes : 1);
        size_t count = 0;
        mpz_export(buffer.data(), &count, 1, 1, 0, 0, value);
        if (count > 0) buffer.resize(count);
        write_u64_le(out, static_cast<uint64_t>(buffer.size()));
        if (!buffer.empty()) out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    };

    writeMpz(genreCode);
    writeMpz(artistCode);
    writeMpz(albumCode);
    writeMpz(trackCode);

    // Write fingerprint blob
    uint64_t fingerprintSize = static_cast<uint64_t>(audioFingerprint.size());
    write_u64_le(out, fingerprintSize);
    if (fingerprintSize > 0) out.write(reinterpret_cast<const char*>(audioFingerprint.data()), fingerprintSize);
}

AudioIndex AudioIndex::deserialize(std::istream& in) {
    AudioIndex index;

    // Read basic properties
    in.read(reinterpret_cast<char*>(&index.sampleRate), sizeof(index.sampleRate));
    in.read(reinterpret_cast<char*>(&index.duration), sizeof(index.duration));
    in.read(reinterpret_cast<char*>(&index.bitDepth), sizeof(index.bitDepth));

    // Read mpz values: read length (u64 LE) then import raw bytes
    auto readMpz = [&in](mpz_t value) {
        uint64_t size64 = 0;
        if (!read_u64_le(in, size64)) return false;
        size_t size = static_cast<size_t>(size64);
        if (size > 0) {
            std::vector<uint8_t> buffer(size);
            if (!in.read(reinterpret_cast<char*>(buffer.data()), size)) return false;
            mpz_import(value, size, 1, 1, 0, 0, buffer.data());
        }
        return true;
    };

    if (!readMpz(index.genreCode)) return index;
    if (!readMpz(index.artistCode)) return index;
    if (!readMpz(index.albumCode)) return index;
    if (!readMpz(index.trackCode)) return index;

    // Read fingerprint
    uint64_t fingerprintSize = 0;
    if (!read_u64_le(in, fingerprintSize)) return index;
    if (fingerprintSize > 0) {
        index.audioFingerprint.resize(static_cast<size_t>(fingerprintSize));
        in.read(reinterpret_cast<char*>(index.audioFingerprint.data()), static_cast<std::streamsize>(fingerprintSize));
    }

    return index;
}

// ---------------------------------------------------------------------------
// 5) Converters / accessors
// ---------------------------------------------------------------------------
std::vector<int32_t> AudioIndex::toAudioSamples() const {
    if (audioFingerprint.empty()) return {};
    AudioFingerprint fingerprint = AudioFingerprint::deserialize(audioFingerprint);
    return fingerprint.toSamples(sampleRate);
}

std::string AudioIndex::getGenreString() const { return mpzToString(genreCode); }
std::string AudioIndex::getArtistString() const { return mpzToString(artistCode); }
std::string AudioIndex::getAlbumString() const { return mpzToString(albumCode); }
std::string AudioIndex::getTrackString() const { return mpzToString(trackCode); }
std::string AudioIndex::getFullPath() const {
    return getGenreString() + "/" + getArtistString() + "/" + getAlbumString() + "/" + getTrackString();
}

// ---------------------------------------------------------------------------
// 6) mpz <-> string helpers (base-94 printable alphabet 33..126)
// ---------------------------------------------------------------------------
bool AudioIndex::stringToMpz(const std::string& str, mpz_t result) const {
    const unsigned long BASE = 94UL;

    // Handle empty string case
    if (str.empty()) {
        mpz_set_ui(result, 0);
        return true;
    }

    // Validate allowed characters
    for (unsigned char c : str) if (c < 33 || c > 126) return false;

    // Initialize result
    mpz_set_ui(result, 0);

    // Convert each character to its base-94 digit
    for (unsigned char c : str) {
        unsigned long digit = static_cast<unsigned long>(c - 33);
        mpz_mul_ui(result, result, BASE);
        mpz_add_ui(result, result, digit);
    }
    return true;
}

std::string AudioIndex::mpzToString(const mpz_t value) const {
    const unsigned long BASE = 94UL;
    if (mpz_cmp_ui(value, 0) == 0) return std::string(1, static_cast<char>(33)); // '!' for zero

    mpz_t tmp, q, d, rem_mpz;
    mpz_init_set(tmp, value);
    mpz_init(q);
    mpz_init_set_ui(d, BASE);
    mpz_init(rem_mpz);

    std::string rev;
    while (mpz_cmp_ui(tmp, 0) > 0) {
        mpz_tdiv_qr(q, rem_mpz, tmp, d);
        unsigned long r = mpz_get_ui(rem_mpz);
        rev.push_back(static_cast<char>(33 + r));
        mpz_set(tmp, q);
    }

    mpz_clear(rem_mpz);
    mpz_clear(d);
    mpz_clear(tmp);
    mpz_clear(q);
    std::reverse(rev.begin(), rev.end());
    return rev;
}

/*
// ---------------------------------------------------------------------------
// 7) Optional browsing helpers
//    (commented out: keep for future exploration)
// ---------------------------------------------------------------------------
std::vector<AudioIndex> AudioIndex::getSimilarGenres(int count) const { ... }
std::vector<AudioIndex> AudioIndex::getArtistsInGenre(int count) const { ... }
std::vector<AudioIndex> AudioIndex::getAlbumsFromArtist(int count) const { ... }
std::vector<AudioIndex> AudioIndex::getTracksFromAlbum(int count) const { ... }
*/

// ---------------------------------------------------------------------------
// 8) Comparison operators
// ---------------------------------------------------------------------------
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
