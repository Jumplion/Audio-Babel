/**
 * URL-safe base64 encoding utilities.
 * Uses the same alphabet as the C++ library: A-Z, a-z, 0-9, -, _ (no padding)
 */

export const ALPHABET = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';

// Converts a room number to a bijective base64 string (no padding), matching
// Utilities::indexToB64 (digit = value + 1). Room numbers must round-trip
// through this same bijective scheme, or reconstructIndex will silently
// resolve to the wrong room.
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
