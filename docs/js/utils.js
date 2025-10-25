/**
 * Utility helpers for base64 encoding and data conversion
 */

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
 * Convert Uint8Array to standard base64 string
 * @param {Uint8Array} bytes - Bytes to encode
 * @returns {string} Standard base64 string
 */
export function bytesToBase64(bytes) {
  let binaryString = '';
  for (let i = 0; i < bytes.length; i++) {
    binaryString += String.fromCharCode(bytes[i]);
  }
  return btoa(binaryString);
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
 * Encode text to URL-safe base64 (for sample-based indexes)
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
