/**
 * audioIndex.js
 * 
 * Wrapper module that uses the WebAssembly/C++ audio indexing library.
 * All heavy lifting is done by the compiled C++ code for performance and consistency.
 * 
 * This module provides a clean interface to the WASM functionality.
 */

import { DEFAULT_SAMPLE_RATE, DEFAULT_BIT_DEPTH, DEFAULT_NUM_CHANNELS } from './audioConstants.js';

// Re-export so page modules can import everything from one place
export { DEFAULT_SAMPLE_RATE, DEFAULT_BIT_DEPTH, DEFAULT_NUM_CHANNELS };

/**
 * Validate that a string contains only URL-safe base64 characters
 * @param {string} s - String to validate
 * @returns {boolean} True if valid
 */
export function isValidBase64Url(s) {
    return /^[A-Za-z0-9\-_]*$/.test(s);
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

/**
 * Build a standardised result object for handleJsonResponse.
 * Callers still append wavBase64 after WAV generation.
 * @param {Object} opts
 * @param {string} opts.indexBase64 - Base64-encoded audio index
 * @param {string} opts.genre - Source genre label
 * @param {string} opts.artist - Source artist label
 * @param {number} opts.pcmDataSize - PCM byte count
 * @param {number} [opts.sampleRate]
 * @param {number} [opts.numChannels]
 * @param {number} [opts.bitDepth]
 * @returns {Object} Result object ready for handleJsonResponse (minus wavBase64)
 */
export function buildResult({ indexBase64, genre, artist, pcmDataSize, sampleRate = DEFAULT_SAMPLE_RATE, numChannels = DEFAULT_NUM_CHANNELS, bitDepth = DEFAULT_BIT_DEPTH }) {
    const duration = calculateDuration(pcmDataSize, sampleRate, bitDepth, numChannels);
    return {
        indexBase64,
        metadata: {
            genre,
            artist,
            album: `${duration.toFixed(2)}s`,
            track: `${(indexBase64.length / 1024).toFixed(2)} KB`,
            cover: ''
        },
        sampleRate,
        numChannels,
        dataSize: pcmDataSize,
        duration
    };
}