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
