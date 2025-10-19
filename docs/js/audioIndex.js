/**
 * audioIndex.js
 * 
 * Pure JavaScript port of the C++ audio indexing library.
 * Handles base64 URL-safe encoding/decoding, metadata extraction,
 * SVG cover generation, and WAV file generation from indexes.
 * 
 * This module enables client-side processing without a server.
 */

// Constants (from cpp/include/Constants.h)
const BITS_PER_BYTE = 8;
const BASE64_BITS = 6;
const BASE64_MASK = 0x3F;
const PCM_FORMAT_CODE = 1;
const DEFAULT_NUM_CHANNELS = 1;
const DEFAULT_SAMPLE_RATE = 44100;
const DEFAULT_BIT_DEPTH = 16;

// Base64 URL-safe alphabet (A-Z a-z 0-9 - _), no padding
const BASE64_URL_ALPHABET = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';

/**
 * Validate that a string contains only URL-safe base64 characters
 * @param {string} s - String to validate
 * @returns {boolean} True if valid
 */
export function isValidBase64Url(s) {
    return /^[A-Za-z0-9\-_]*$/.test(s);
}

/**
 * Decode URL-safe base64 (no padding) into a Uint8Array
 * @param {string} s - Base64 string to decode
 * @returns {Uint8Array} Decoded bytes
 * @throws {Error} If string contains invalid characters
 */
export function decodeBase64Url(s) {
    if (!isValidBase64Url(s)) {
        throw new Error('Invalid base64 character in input');
    }

    // Build reverse lookup table
    const rev = new Array(256).fill(-1);
    for (let i = 0; i < BASE64_URL_ALPHABET.length; i++) {
        rev[BASE64_URL_ALPHABET.charCodeAt(i)] = i;
    }

    const out = [];
    let acc = 0;
    let accBits = 0;

    for (let i = 0; i < s.length; i++) {
        const v = rev[s.charCodeAt(i)];
        if (v < 0) {
            throw new Error('Invalid base64 character in input');
        }
        acc = (acc << 6) | v;
        accBits += 6;
        if (accBits >= 8) {
            accBits -= 8;
            out.push((acc >> accBits) & 0xFF);
        }
    }

    return new Uint8Array(out);
}

/**
 * Encode bytes into URL-safe base64 (no padding)
 * @param {Uint8Array} bytes - Bytes to encode
 * @returns {string} Base64 encoded string
 */
export function encodeBase64Url(bytes) {
    let result = '';
    let acc = 0;
    let accBits = 0;

    for (const byte of bytes) {
        acc = (acc << BITS_PER_BYTE) | byte;
        accBits += BITS_PER_BYTE;
        while (accBits >= BASE64_BITS) {
            accBits -= BASE64_BITS;
            const idx = (acc >> accBits) & BASE64_MASK;
            result += BASE64_URL_ALPHABET[idx];
        }
    }

    if (accBits > 0) {
        const idx = (acc << (BASE64_BITS - accBits)) & BASE64_MASK;
        result += BASE64_URL_ALPHABET[idx];
    }

    return result;
}

/**
 * Generate an SVG cover from bytes and track name
 * @param {Uint8Array} bytes - Index bytes
 * @param {string} track - Track name to display
 * @returns {string} SVG markup
 */
export function generateSvgCover(bytes, track) {
    // Extract color from first 3 bytes
    let color = 0;
    for (let i = 0; i < 3; i++) {
        color = (color << 8) | (i < bytes.length ? bytes[i] : 0);
    }

    const colorHex = color.toString(16).padStart(6, '0');

    return `<svg xmlns='http://www.w3.org/2000/svg' width='256' height='256'>` +
           `<rect width='100%' height='100%' fill='#${colorHex}'/>` +
           `<text x='50%' y='50%' font-size='20' text-anchor='middle' fill='#fff' dominant-baseline='middle'>${track}</text>` +
           `</svg>`;
}

/**
 * Extract metadata from index bytes and base64 string
 * (Ports buildMetadataFromBytesAndB64 from IndexMetadata.cpp)
 * @param {Uint8Array} bytes - Raw index bytes
 * @param {string} b64str - Base64 representation
 * @returns {Object} Metadata object with genre, artist, album, track, cover
 */
function buildMetadataFromBytesAndB64(bytes, b64str) {
    const meta = {
        genre: 'g0',
        artist: 'a0',
        album: 'al0',
        track: 't0',
        cover: ''
    };

    if (!b64str || b64str.length === 0) {
        return meta;
    }

    // Calculate weights from byte sums
    const weights = [0, 0, 0, 0];
    for (let i = 0; i < bytes.length; i++) {
        weights[i % 4] += bytes[i];
    }

    let totalWeight = weights.reduce((a, b) => a + b, 0);
    if (totalWeight === 0) {
        weights.fill(1);
        totalWeight = 4;
    }

    // Calculate field lengths proportional to weights
    const b64Len = b64str.length;
    const lens = [0, 0, 0, 0];

    let sum = 0;
    for (let i = 0; i < 4; i++) {
        lens[i] = Math.floor((b64Len * weights[i]) / totalWeight);
        if (lens[i] === 0) {
            lens[i] = 1;
        }
        sum += lens[i];
    }

    // Adjust lengths to match exact string length
    for (let i = 0; sum < b64Len; i = (i + 1) % 4) {
        lens[i]++;
        sum++;
    }

    for (let i = 3; sum > b64Len && i >= 0; i--) {
        if (lens[i] > 1) {
            lens[i]--;
            sum--;
        }
        if (i === 0 && sum > b64Len) {
            i = 4; // Wraps back to 3 on next iteration
        }
    }

    // Extract substrings
    let pos = 0;
    meta.genre = b64str.substring(pos, pos + lens[0]);
    pos += lens[0];
    meta.artist = pos < b64Len ? b64str.substring(pos, pos + lens[1]) : '';
    pos += lens[1];
    meta.album = pos < b64Len ? b64str.substring(pos, pos + lens[2]) : '';
    pos += lens[2];
    meta.track = pos < b64Len ? b64str.substring(pos, pos + lens[3]) : '';

    // Generate SVG cover
    meta.cover = generateSvgCover(bytes, meta.track);

    return meta;
}

/**
 * Extract metadata from a base64 index string
 * @param {string} base64Index - URL-safe base64 index
 * @returns {Object} Metadata object
 */
export function extractMetadataFromIndex(base64Index) {
    const bytes = decodeBase64Url(base64Index);
    return buildMetadataFromBytesAndB64(bytes, base64Index);
}

/**
 * Generate a WAV file from index bytes
 * @param {Uint8Array} indexBytes - Raw PCM sample data
 * @param {number} sampleRate - Sample rate (default 44100)
 * @param {number} bitDepth - Bit depth (default 16)
 * @param {number} numChannels - Number of channels (default 1/mono)
 * @returns {Blob} WAV file as a Blob
 */
export function generateWavFromBytes(indexBytes, sampleRate = DEFAULT_SAMPLE_RATE, bitDepth = DEFAULT_BIT_DEPTH, numChannels = DEFAULT_NUM_CHANNELS) {
    const dataSize = indexBytes.length;
    const byteRate = sampleRate * numChannels * (bitDepth / 8);
    const blockAlign = numChannels * (bitDepth / 8);
    const fileSize = 36 + dataSize;

    // Create buffer for WAV file (44-byte header + data)
    const buffer = new ArrayBuffer(44 + dataSize);
    const view = new DataView(buffer);

    // Helper to write ASCII string
    const writeString = (offset, str) => {
        for (let i = 0; i < str.length; i++) {
            view.setUint8(offset + i, str.charCodeAt(i));
        }
    };

    // RIFF header
    writeString(0, 'RIFF');
    view.setUint32(4, fileSize, true); // little-endian
    writeString(8, 'WAVE');

    // fmt chunk
    writeString(12, 'fmt ');
    view.setUint32(16, 16, true); // fmt chunk size
    view.setUint16(20, PCM_FORMAT_CODE, true); // PCM
    view.setUint16(22, numChannels, true);
    view.setUint32(24, sampleRate, true);
    view.setUint32(28, byteRate, true);
    view.setUint16(32, blockAlign, true);
    view.setUint16(34, bitDepth, true);

    // data chunk
    writeString(36, 'data');
    view.setUint32(40, dataSize, true);

    // Copy PCM data
    new Uint8Array(buffer, 44).set(indexBytes);

    return new Blob([buffer], { type: 'audio/wav' });
}

/**
 * Decode a base64 index and generate a WAV file
 * @param {string} base64Index - URL-safe base64 index
 * @returns {Object} Object with metadata and wav Blob
 */
export function decodeIndex(base64Index) {
    const bytes = decodeBase64Url(base64Index);
    const metadata = buildMetadataFromBytesAndB64(bytes, base64Index);
    const wav = generateWavFromBytes(bytes);

    return {
        metadata,
        wav,
        bytes
    };
}

/**
 * Encode a WAV file into a base64 index
 * Extracts PCM data from WAV and converts to base64
 * @param {ArrayBuffer} wavBuffer - WAV file as ArrayBuffer
 * @returns {Object} Object with base64Index and metadata
 */
export function encodeWavFile(wavBuffer) {
    const view = new DataView(wavBuffer);

    // Verify RIFF/WAVE header
    const riff = String.fromCharCode(view.getUint8(0), view.getUint8(1), view.getUint8(2), view.getUint8(3));
    const wave = String.fromCharCode(view.getUint8(8), view.getUint8(9), view.getUint8(10), view.getUint8(11));
    
    if (riff !== 'RIFF' || wave !== 'WAVE') {
        throw new Error('Invalid WAV file: missing RIFF/WAVE header');
    }

    let offset = 12;
    let pcmData = null;
    let audioFormat = null;
    let numChannels = null;
    let sampleRate = null;
    let bitDepth = null;

    // Parse chunks
    while (offset < view.byteLength - 8) {
        const chunkId = String.fromCharCode(
            view.getUint8(offset),
            view.getUint8(offset + 1),
            view.getUint8(offset + 2),
            view.getUint8(offset + 3)
        );
        const chunkSize = view.getUint32(offset + 4, true);
        offset += 8;

        if (chunkId === 'fmt ') {
            audioFormat = view.getUint16(offset, true);
            numChannels = view.getUint16(offset + 2, true);
            sampleRate = view.getUint32(offset + 4, true);
            bitDepth = view.getUint16(offset + 14, true);
            offset += chunkSize;
        } else if (chunkId === 'data') {
            pcmData = new Uint8Array(wavBuffer, offset, chunkSize);
            offset += chunkSize;
        } else {
            // Skip unknown chunk
            offset += chunkSize + (chunkSize & 1); // Account for padding
        }
    }

    if (!pcmData) {
        throw new Error('No data chunk found in WAV file');
    }

    // Encode PCM data to base64
    const base64Index = encodeBase64Url(pcmData);
    const metadata = buildMetadataFromBytesAndB64(pcmData, base64Index);

    return {
        base64Index,
        metadata,
        audioFormat,
        numChannels,
        sampleRate,
        bitDepth,
        dataSize: pcmData.length
    };
}

/**
 * Generate a random base64 index of specified length
 * @param {number} numBytes - Number of random bytes to generate
 * @returns {string} Random base64 index
 */
export function generateRandomIndex(numBytes = 1000) {
    const bytes = new Uint8Array(numBytes);
    crypto.getRandomValues(bytes);
    return encodeBase64Url(bytes);
}

/**
 * Calculate the duration of audio from byte count and format
 * @param {number} numBytes - Number of PCM bytes
 * @param {number} sampleRate - Sample rate
 * @param {number} bitDepth - Bit depth
 * @param {number} numChannels - Number of channels
 * @returns {number} Duration in seconds
 */
export function calculateDuration(numBytes, sampleRate = DEFAULT_SAMPLE_RATE, bitDepth = DEFAULT_BIT_DEPTH, numChannels = DEFAULT_NUM_CHANNELS) {
    const bytesPerSample = bitDepth / 8;
    const numSamples = numBytes / (bytesPerSample * numChannels);
    return numSamples / sampleRate;
}
