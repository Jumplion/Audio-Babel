// src/services/indexParser.js
import { getPositionFromIndex, parseAddress } from './positionEncoder.js';

/**
 * Parse a base64 URL-safe index into its components
 * @param {string} base64Index - The base64-encoded audio index
 * @returns {object} Parsed index data including position and metadata
 */
function parseIndex(base64Index) {
    if (!isValidBase64Url(base64Index)) {
        throw new Error('Invalid index format. Must be URL-safe base64 without padding.');
    }

    // Get position information
    const position = getPositionFromIndex(base64Index);
    
    // Extract metadata from index (genre, artist, album, track labels)
    // These are derived from segments of the base64 string itself
    const metadata = extractMetadataFromIndex(base64Index);

    return {
        index: base64Index,
        position,
        metadata,
        address: position.address
    };
}

/**
 * Extract metadata segments from the base64 index
 * This mimics the C++ implementation's metadata extraction
 * @param {string} base64Index - The base64-encoded audio index
 * @returns {object} Metadata segments
 */
function extractMetadataFromIndex(base64Index) {
    if (!base64Index || base64Index.length === 0) {
        return {
            genre: 'g0',
            artist: 'a0',
            album: 'al0',
            track: 't0'
        };
    }

    // Calculate weights from index characters (simplified version of C++ algorithm)
    const weights = [0, 0, 0, 0];
    for (let i = 0; i < base64Index.length; i++) {
        const charCode = base64Index.charCodeAt(i);
        weights[i % 4] += charCode;
    }

    const totalWeight = weights.reduce((a, b) => a + b, 0) || 4;
    
    // Calculate segment lengths proportional to weights
    const lengths = weights.map(w => {
        let len = Math.floor((base64Index.length * w) / totalWeight);
        return len > 0 ? len : 1;
    });

    // Adjust lengths to match total index length
    let sum = lengths.reduce((a, b) => a + b, 0);
    let i = 0;
    while (sum < base64Index.length) {
        lengths[i % 4]++;
        sum++;
        i++;
    }
    while (sum > base64Index.length) {
        for (let j = 3; j >= 0 && sum > base64Index.length; j--) {
            if (lengths[j] > 1) {
                lengths[j]--;
                sum--;
            }
        }
    }

    // Extract segments
    let pos = 0;
    const genre = base64Index.substring(pos, pos + lengths[0]);
    pos += lengths[0];
    const artist = base64Index.substring(pos, pos + lengths[1]);
    pos += lengths[1];
    const album = base64Index.substring(pos, pos + lengths[2]);
    pos += lengths[2];
    const track = base64Index.substring(pos, pos + lengths[3]);

    return { genre, artist, album, track };
}

/**
 * Validate that a string is URL-safe base64 (no padding)
 * @param {string} str - String to validate
 * @returns {boolean} True if valid
 */
function isValidBase64Url(str) {
    // URL-safe base64 alphabet: A-Z, a-z, 0-9, -, _
    // No padding (=) allowed
    const regex = /^[A-Za-z0-9_-]+$/;
    return regex.test(str);
}

/**
 * Generate a display-friendly name from metadata
 * @param {object} metadata - Metadata object with genre, artist, album, track
 * @returns {object} Display names
 */
function generateDisplayNames(metadata) {
    return {
        genreName: `Genre: ${metadata.genre}`,
        artistName: `Artist: ${metadata.artist}`,
        albumName: `Album: ${metadata.album}`,
        trackName: `Track: ${metadata.track}`
    };
}

/**
 * Parse an address string to get navigation coordinates
 * @param {string} address - Address in format room_HASH:W#:S##:T##
 * @returns {object} Navigation coordinates
 */
function parseAddressString(address) {
    return parseAddress(address);
}

/**
 * Validate index against audio constraints
 * @param {string} base64Index - The base64-encoded audio index
 * @param {number} expectedMaxLength - Maximum expected length
 * @returns {boolean} True if valid
 */
function validateIndex(base64Index, expectedMaxLength = null) {
    if (!isValidBase64Url(base64Index)) {
        throw new Error('Invalid index format. Must be URL-safe base64.');
    }
    
    if (expectedMaxLength && base64Index.length > expectedMaxLength) {
        throw new Error(`Index too long: ${base64Index.length} > ${expectedMaxLength}`);
    }
    
    return true;
}

/**
 * Get a short preview of an index for display
 * @param {string} base64Index - The base64-encoded audio index
 * @param {number} maxLength - Maximum preview length
 * @returns {string} Preview string
 */
function getIndexPreview(base64Index, maxLength = 16) {
    if (base64Index.length <= maxLength) {
        return base64Index;
    }
    const start = base64Index.substring(0, maxLength / 2);
    const end = base64Index.substring(base64Index.length - maxLength / 2);
    return `${start}...${end}`;
}

export { 
    parseIndex, 
    extractMetadataFromIndex,
    isValidBase64Url, 
    validateIndex,
    generateDisplayNames,
    parseAddressString,
    getIndexPreview
};