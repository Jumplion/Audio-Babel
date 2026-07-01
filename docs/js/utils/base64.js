/**
 * URL-safe base64 encoding utilities.
 * Uses the same alphabet as the C++ library: A-Z, a-z, 0-9, -, _ (no padding)
 */

export const ALPHABET = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';

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
