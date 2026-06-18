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

console.info('✅ validationUtils.js loaded - validation utilities ready');
