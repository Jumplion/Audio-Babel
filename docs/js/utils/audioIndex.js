/**
 * audioIndex.js
 * 
 * Wrapper module that uses the WebAssembly/C++ audio indexing library.
 * All heavy lifting is done by the compiled C++ code for performance and consistency.
 * 
 * This module provides a clean interface to the WASM functionality.
 */

import AudioIndexWASM from '../core/audioIndexWasm.js';

// Constants (from cpp/include/Constants.h)
const DEFAULT_NUM_CHANNELS = 1;
const DEFAULT_SAMPLE_RATE = 44100;
const DEFAULT_BIT_DEPTH = 16;

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