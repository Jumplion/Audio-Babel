/**
 * URL-safe base64 encoding/decoding utilities.
 * Uses the same alphabet as the C++ library: A-Z, a-z, 0-9, -, _ (no padding)
 */

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
 * Convert a room number to a bijective base64 string (no padding).
 * Used for encoding room numbers in the library hierarchy.
 *
 * NOTE: this is the BIJECTIVE base64 used by the C++ index/room encoding
 * (digit = value + 1; see Utilities::indexToB64), NOT the standard bit-packing
 * base64 above. LibraryPosition::calculateLibraryPosition encodes pos.room via
 * Utilities::indexToB64, and reconstructIndexFromPosition decodes it via
 * Utilities::b64ToIndex — so room numbers typed here must round-trip through
 * the same bijective scheme or reconstructIndex will silently resolve to the
 * wrong room.
 * @param {BigInt} index - Room number to encode
 * @returns {string} Bijective base64 URL-safe encoded string
 */
export function indexToBase64(index) {
  const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
  let n = BigInt(index);
  if (n === 0n) return ''; // Room 0 is encoded as the empty string

  const digits = [];
  while (n > 0n) {
    n -= 1n;
    digits.push(Number(n % 64n));
    n /= 64n;
  }
  digits.reverse();

  return digits.map((d) => alphabet[d]).join('');
}
