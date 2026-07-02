/**
 * Input validation utilities for form fields.
 */

// Intentionally duplicated from C++ Utilities::isValidBase64Url: JS validates
// at the UI boundary, C++ validates at the library boundary.
export function isValidBase64Url(s) {
  return /^[A-Za-z0-9\-_]*$/.test(s);
}

export function filterToBase64UrlChars(text) {
  return text.replace(/[^A-Za-z0-9\-_]/g, '');
}

console.info('✅ validationUtils.js loaded - validation utilities ready');
