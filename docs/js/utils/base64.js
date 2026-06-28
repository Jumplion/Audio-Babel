/**
 * URL-safe base64 encoding utilities.
 * Uses the same alphabet as the C++ library: A-Z, a-z, 0-9, -, _ (no padding)
 */

const ALPHABET = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';

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
 * Decode a standard base64 string into raw bytes.
 * @param {string} base64 - Standard (non-URL-safe) base64 string
 * @returns {Uint8Array} Decoded bytes
 */
export function base64ToBytes(base64) {
  const binaryString = atob(base64);
  const bytes = new Uint8Array(binaryString.length);
  for (let i = 0; i < binaryString.length; i++) {
    bytes[i] = binaryString.charCodeAt(i);
  }
  return bytes;
}

/**
 * Convert a room number to a bijective base64 string (no padding).
 * Used for encoding room numbers in the library hierarchy.
 *
 * NOTE: this is the BIJECTIVE base64 used by the C++ index/room encoding
 * (digit = value + 1; see Utilities::indexToB64). LibraryPosition::calculateLibraryPosition
 * encodes pos.room via Utilities::indexToB64, and reconstructIndexFromPosition decodes it via
 * Utilities::b64ToIndex — so room numbers typed here must round-trip through
 * the same bijective scheme or reconstructIndex will silently resolve to the
 * wrong room.
 * @param {BigInt} index - Room number to encode
 * @returns {string} Bijective base64 URL-safe encoded string
 */
export function indexToBase64(index) {
  let n = BigInt(index);
  if (n === 0n) return ''; // Room 0 is encoded as the empty string

  const digits = [];
  while (n > 0n) {
    n -= 1n;
    digits.push(Number(n % 64n));
    n /= 64n;
  }
  digits.reverse();

  return digits.map((d) => ALPHABET[d]).join('');
}
