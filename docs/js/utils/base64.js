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
 * Shared helper: extract the first `length` base64 characters (6 bits each)
 * from a byte stream, MSB-first. Assumes `bytes` holds at least `ceil(6*length/8)` bytes.
 * @param {Uint8Array} bytes - Source bytes
 * @param {number} length - Number of 6-bit characters to extract
 * @param {string} alphabet - Base64 alphabet to use
 * @returns {string} Extracted characters
 */
function extractIndexChars(bytes, length, alphabet) {
  let result = '';
  let acc = 0;
  let accBits = 0;
  let byteIndex = 0;

  for (let i = 0; i < length; i++) {
    while (accBits < 6) {
      acc = (acc << 8) | bytes[byteIndex++];
      accBits += 8;
    }
    accBits -= 6;
    result += alphabet[(acc >> accBits) & 0x3F];
  }

  return result;
}

/**
 * Decode a user-facing index string of any length (including empty) into bytes,
 * losslessly - every distinct input string maps to a distinct byte array.
 *
 * The trailing partial 6-bit group (if any) is rounded *up* to a whole byte
 * (zero-padded in the low bits) instead of being discarded, and a final marker
 * byte storing `indexString.length % 4` is appended. The marker disambiguates
 * strings whose rounded-up data would otherwise collide (e.g. "AAA" vs "AAAA").
 *
 * @param {string} indexString - URL-safe base64 string (any length, A-Z a-z 0-9 - _)
 * @returns {Uint8Array} Rounded-up decoded bytes followed by a 1-byte length marker
 * @throws {Error} If invalid base64 characters are encountered
 */
export function decodeIndexString(indexString) {
  const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
  const lookup = new Map();
  for (let i = 0; i < alphabet.length; i++) {
    lookup.set(alphabet[i], i);
  }

  const length = indexString.length;
  const byteCount = Math.ceil((6 * length) / 8);
  const bytes = new Uint8Array(byteCount + 1);

  let acc = 0;
  let accBits = 0;
  let byteIndex = 0;

  for (let i = 0; i < length; i++) {
    const char = indexString[i];
    const value = lookup.get(char);

    if (value === undefined) {
      throw new Error(`Invalid base64 character: '${char}' at position ${i}`);
    }

    acc = (acc << 6) | value;
    accBits += 6;

    if (accBits >= 8) {
      accBits -= 8;
      bytes[byteIndex++] = (acc >> accBits) & 0xFF;
    }
  }

  // Round the leftover partial group up to a whole byte (zero-padded low bits)
  if (accBits > 0) {
    bytes[byteIndex++] = (acc << (8 - accBits)) & 0xFF;
  }

  // Marker byte: how many characters past the last full 4-char/3-byte group
  bytes[byteIndex] = length % 4;

  return bytes;
}

/**
 * Inverse of decodeIndexString: reconstruct the exact original index string
 * from bytes produced by decodeIndexString (rounded-up data + trailing marker byte).
 *
 * @param {Uint8Array} bytes - Bytes including the trailing length marker
 * @returns {string} The original URL-safe base64 index string
 */
export function encodeIndexBytes(bytes) {
  const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
  const remainder = bytes[bytes.length - 1] & 0x03;
  const data = bytes.subarray(0, bytes.length - 1);
  const dataByteCount = data.length;

  // Recover the 4-char/3-byte group count from the rounded-up byte count and the remainder
  let groupCount;
  switch (remainder) {
    case 0: groupCount = dataByteCount / 3; break;
    case 1: groupCount = (dataByteCount - 1) / 3; break;
    case 2: groupCount = (dataByteCount - 2) / 3; break;
    default: groupCount = (dataByteCount / 3) - 1; break; // remainder === 3
  }

  const length = 4 * groupCount + remainder;
  return extractIndexChars(data, length, alphabet);
}

/**
 * Encode raw bytes (no marker byte) into a canonical index string.
 * Used for the audio -> index direction (uploaded WAV files, random audio),
 * where there is no marker byte to recover the original string length from -
 * `r = byteCount % 3` is picked directly to produce a well-defined index.
 *
 * @param {Uint8Array} bytes - Raw PCM bytes
 * @returns {string} URL-safe base64 index string
 */
export function encodeCanonicalIndexString(bytes) {
  const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
  const byteCount = bytes.length;
  const remainder = byteCount % 3;

  let groupCount, charRemainder;
  if (remainder === 0) {
    charRemainder = 0;
    groupCount = byteCount / 3;
  } else if (remainder === 1) {
    charRemainder = 1;
    groupCount = (byteCount - 1) / 3;
  } else {
    charRemainder = 2;
    groupCount = (byteCount - 2) / 3;
  }

  const length = 4 * groupCount + charRemainder;
  return extractIndexChars(bytes, length, alphabet);
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
