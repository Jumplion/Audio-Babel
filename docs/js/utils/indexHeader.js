/**
 * Helpers for adding/removing/parsing the 13-byte audio index header.
 *
 * Header format (13 bytes, little-endian):
 *   Byte 0:      VERSION (0x01)
 *   Byte 1-4:    num_frames (uint32_t)
 *   Byte 5-8:    sample_rate (uint32_t)
 *   Byte 9-10:   bit_depth (uint16_t)
 *   Byte 11-12:  num_channels (uint16_t)
 */

import { decodeBase64Url, encodeBase64Url } from './base64.js';

/**
 * Strip the 13-byte header from a full audio index
 * Converts from internal format (with header) to user-facing format (PCM only)
 * @param {string} fullIndexBase64 - Full index with header (URL-safe base64)
 * @returns {string} User-facing index (PCM data only, URL-safe base64)
 */
export function stripIndexHeader(fullIndexBase64) {
  try {
    const bytes = decodeBase64Url(fullIndexBase64);

    // Check if this has a version byte (0x01 = version 1 with header)
    if (bytes.length >= 13 && bytes[0] === 0x01) {
      // Strip the first 13 bytes (header)
      const pcmOnly = bytes.slice(13);
      return encodeBase64Url(pcmOnly);
    }

    // No header detected, return as-is
    return fullIndexBase64;
  } catch (error) {
    console.error('Error stripping index header:', error);
    return fullIndexBase64; // Return original on error
  }
}

/**
 * Add 13-byte header to PCM-only audio index
 * Converts from user-facing format (PCM only) to internal format (with header)
 *
 * @param {string} userIndexBase64 - User-facing index (PCM data only, URL-safe base64)
 * @param {Object} audioParams - Audio parameters
 * @param {number} audioParams.numFrames - Number of audio frames
 * @param {number} audioParams.sampleRate - Sample rate in Hz (default: 44100)
 * @param {number} audioParams.bitDepth - Bit depth (default: 16)
 * @param {number} audioParams.numChannels - Number of channels (default: 1)
 * @returns {string} Full index with header (URL-safe base64)
 */
export function addIndexHeader(userIndexBase64, audioParams) {
  const {
    numFrames,
    sampleRate = 44100,
    bitDepth = 16,
    numChannels = 1
  } = audioParams;

  if (numFrames === undefined) {
    throw new Error('numFrames is required in audioParams');
  }

  try {
    const pcmBytes = decodeBase64Url(userIndexBase64);

    // Build 13-byte header (little-endian)
    const header = new Uint8Array(13);

    // Byte 0: VERSION (0x01)
    header[0] = 0x01;

    // Bytes 1-4: num_frames (uint32_t, little-endian)
    header[1] = numFrames & 0xFF;
    header[2] = (numFrames >> 8) & 0xFF;
    header[3] = (numFrames >> 16) & 0xFF;
    header[4] = (numFrames >> 24) & 0xFF;

    // Bytes 5-8: sample_rate (uint32_t, little-endian)
    header[5] = sampleRate & 0xFF;
    header[6] = (sampleRate >> 8) & 0xFF;
    header[7] = (sampleRate >> 16) & 0xFF;
    header[8] = (sampleRate >> 24) & 0xFF;

    // Bytes 9-10: bit_depth (uint16_t, little-endian)
    header[9] = bitDepth & 0xFF;
    header[10] = (bitDepth >> 8) & 0xFF;

    // Bytes 11-12: num_channels (uint16_t, little-endian)
    header[11] = numChannels & 0xFF;
    header[12] = (numChannels >> 8) & 0xFF;

    // Concatenate header + PCM data
    const fullBytes = new Uint8Array(13 + pcmBytes.length);
    fullBytes.set(header, 0);
    fullBytes.set(pcmBytes, 13);

    return encodeBase64Url(fullBytes);
  } catch (error) {
    console.error('Error adding index header:', error);
    throw error;
  }
}

/**
 * Parse header from a full audio index
 * Extracts metadata without decoding the entire index
 * @param {string} fullIndexBase64 - Full index with header (URL-safe base64)
 * @returns {Object|null} Header info or null if no valid header
 */
export function parseIndexHeader(fullIndexBase64) {
  try {
    const bytes = decodeBase64Url(fullIndexBase64);

    if (bytes.length < 13 || bytes[0] !== 0x01) {
      return null; // No valid header
    }

    // Read header fields (little-endian)
    const numFrames = bytes[1] | (bytes[2] << 8) | (bytes[3] << 16) | (bytes[4] << 24);
    const sampleRate = bytes[5] | (bytes[6] << 8) | (bytes[7] << 16) | (bytes[8] << 24);
    const bitDepth = bytes[9] | (bytes[10] << 8);
    const numChannels = bytes[11] | (bytes[12] << 8);

    return {
      version: bytes[0],
      numFrames,
      sampleRate,
      bitDepth,
      numChannels,
      duration: numFrames / sampleRate,
      pcmByteCount: bytes.length - 13
    };
  } catch (error) {
    console.error('Error parsing index header:', error);
    return null;
  }
}
