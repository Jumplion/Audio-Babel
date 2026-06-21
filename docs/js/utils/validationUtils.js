/**
 * validationUtils.js
 *
 * Input validation utilities for form fields.
 */

/**
 * Validate that a string contains only URL-safe base64 characters.
 * NOTE (R7): Intentionally duplicated from C++ Utilities::isValidBase64Url.
 * JS validates at the UI boundary; C++ validates at the library boundary.
 * @param {string} s - String to validate
 * @returns {boolean} True if valid
 */
export function isValidBase64Url(s) {
  return /^[A-Za-z0-9\-_]*$/.test(s);
}

/**
 * Strip any characters that aren't URL-safe base64 (A-Z, a-z, 0-9, -, _).
 * @param {string} text - Text to filter
 * @returns {string} Filtered text containing only valid characters
 */
export function filterToBase64UrlChars(text) {
  return text.replace(/[^A-Za-z0-9\-_]/g, '');
}

console.info('✅ validationUtils.js loaded - validation utilities ready');
