/**
 * Utility helpers for base64 encoding and data conversion
 */

/**
 * Escape HTML special characters to prevent XSS attacks
 * @param {string|number} s - String to escape
 * @returns {string} HTML-escaped string
 */
export function escapeHtml(s) {
  if (!s && s !== 0) return '';
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

/**
 * Convert index BigInt to base64 URL-safe string (no padding)
 * Used for encoding room numbers in the library hierarchy
 * @param {BigInt} index - BigInt to encode
 * @returns {string} Base64 URL-safe encoded string
 */
export function indexToBase64(index) {
  const idx = BigInt(index);
  if (idx === 0n) return ''; // Room 0 is encoded as the empty string
  
  // Convert BigInt to bytes (big-endian)
  const bytes = [];
  let temp = idx;
  while (temp > 0n) {
    bytes.unshift(Number(temp & 0xFFn));
    temp = temp >> 8n;
  }
  
  return encodeBase64Url(new Uint8Array(bytes));
}

/**
 * Convert ArrayBuffer/Uint8Array to standard base64 string
 * @param {ArrayBuffer|Uint8Array} buffer - Data to encode
 * @returns {string} Standard base64 string
 */
export function toBase64(buffer) {
  const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
  let binary = "";
  const chunk = 0x8000;
  for (let i = 0; i < bytes.length; i += chunk) {
    binary += String.fromCharCode.apply(null, bytes.subarray(i, i + chunk));
  }
  return btoa(binary);
}

/**
 * Convert standard base64 string to Uint8Array
 * @param {string} b64 - Standard base64 string
 * @returns {Uint8Array} Decoded bytes
 */
export function base64ToBytes(b64) {
  const binaryString = atob(b64);
  const bytes = new Uint8Array(binaryString.length);
  for (let i = 0; i < binaryString.length; i++) {
    bytes[i] = binaryString.charCodeAt(i);
  }
  return bytes;
}

/**
 * Convert Uint8Array to base64 in chunks (for large data)
 * More efficient for large arrays, prevents stack overflow
 * @param {Uint8Array} bytes - Bytes to encode
 * @param {number} chunkSize - Size of chunks (default: 0x8000)
 * @returns {string} Standard base64 string
 */
export function bytesToBase64Chunked(bytes, chunkSize = 0x8000) {
  let binaryString = '';
  for (let i = 0; i < bytes.length; i += chunkSize) {
    const chunk = bytes.subarray(i, i + chunkSize);
    binaryString += String.fromCharCode.apply(null, chunk);
  }
  return btoa(binaryString);
}

/**
 * Encode text to URL-safe base64
 * @param {string} text - Text to encode
 * @returns {string} URL-safe base64 string (A-Z, a-z, 0-9, -, _)
 */
export function textToBase64Url(text) {
  const encoder = new TextEncoder();
  const bytes = encoder.encode(text);
  
  // Convert to standard base64
  let binaryString = '';
  for (let i = 0; i < bytes.length; i++) {
    binaryString += String.fromCharCode(bytes[i]);
  }
  const base64 = btoa(binaryString);
  
  // Convert to URL-safe base64 (replace +/= with -_)
  return base64.replace(/\+/g, '-').replace(/\//g, '_').replace(/=/g, '');
}

/**
 * Encode Uint8Array to URL-safe base64 (no padding)
 * Uses the same alphabet as the C++ library: A-Z, a-z, 0-9, -, _
 * @param {Uint8Array} bytes - Bytes to encode
 * @returns {string} URL-safe base64 string (no padding)
 */
export function encodeBase64Url(bytes) {
  const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
  let result = '';
  let acc = 0;
  let accBits = 0;
  
  for (let i = 0; i < bytes.length; i++) {
    acc = (acc << 8) | bytes[i];
    accBits += 8;
    
    while (accBits >= 6) {
      accBits -= 6;
      const idx = (acc >> accBits) & 0x3F;
      result += alphabet[idx];
    }
  }
  
  // Handle remaining bits (no padding)
  if (accBits > 0) {
    const idx = (acc << (6 - accBits)) & 0x3F;
    result += alphabet[idx];
  }
  
  return result;
}

/**
 * Decode URL-safe base64 (no padding) to Uint8Array
 * Uses the same alphabet as the C++ library: A-Z, a-z, 0-9, -, _
 * @param {string} base64Url - URL-safe base64 string (no padding)
 * @returns {Uint8Array} Decoded bytes
 * @throws {Error} If invalid base64 characters are encountered
 */
export function decodeBase64Url(base64Url) {
  const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
  const lookup = new Map();
  for (let i = 0; i < alphabet.length; i++) {
    lookup.set(alphabet[i], i);
  }
  
  const result = [];
  let acc = 0;
  let accBits = 0;
  
  for (let i = 0; i < base64Url.length; i++) {
    const char = base64Url[i];
    const value = lookup.get(char);
    
    if (value === undefined) {
      throw new Error(`Invalid base64 character: '${char}' at position ${i}`);
    }
    
    acc = (acc << 6) | value;
    accBits += 6;
    
    if (accBits >= 8) {
      accBits -= 8;
      result.push((acc >> accBits) & 0xFF);
    }
  }
  
  return new Uint8Array(result);
}

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
 * Header format (13 bytes, little-endian):
 *   Byte 0:      VERSION (0x01)
 *   Byte 1-4:    num_frames (uint32_t)
 *   Byte 5-8:    sample_rate (uint32_t)
 *   Byte 9-10:   bit_depth (uint16_t)
 *   Byte 11-12:  num_channels (uint16_t)
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

