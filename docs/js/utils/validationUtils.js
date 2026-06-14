/**
 * validationUtils.js
 * 
 * Input validation utilities for form fields.
 * Provides reusable validation functions with visual feedback.
 */

// Validation thresholds for audio size inputs (in KB)
export const HARD_MAX_KB = 61440;      // 60 MB - hard limit
const RECOMMENDED_MAX_KB = 51200; // 50 MB - soft warning before the hard limit

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
 * Create a size validator for min/max input fields
 * @param {Object} config - Configuration object
 * @param {HTMLInputElement} config.minSizeInput - Min size input element
 * @param {HTMLInputElement} config.maxSizeInput - Max size input element
 * @param {HTMLElement} config.minSizeWarning - Min size warning display element
 * @param {HTMLElement} config.maxSizeWarning - Max size warning display element
 * @param {HTMLElement} config.rangeError - Range error display element
 * @returns {Object} Validator object with validate() method
 */
export function createSizeValidator({
  minSizeInput,
  maxSizeInput,
  minSizeWarning,
  maxSizeWarning,
  rangeError
}) {
  /**
   * Validate a single size input field
   * @param {HTMLInputElement} input - Input element to validate
   * @param {HTMLElement} warning - Warning display element
   * @param {number} value - Parsed value from input
   * @returns {Object} Validation result with hasError and hasWarning flags
   */
  function validateSizeInput(input, warning, value) {
    let hasError = false;
    let hasWarning = false;

    if (input.value && !isNaN(value)) {
      if (value > HARD_MAX_KB) {
        warning.textContent = `⚠ Error: ${value} KB exceeds maximum allowed (60 MB)`;
        warning.style.display = 'block';
        warning.style.color = '#ff4444';
        input.classList.add('error');
        input.classList.remove('warning');
        hasError = true;
      } else if (value > RECOMMENDED_MAX_KB) {
        warning.textContent = `⚠ Warning: ${value} KB exceeds recommended maximum (50 MB)`;
        warning.style.display = 'block';
        warning.style.color = '#ffaa00';
        input.classList.add('warning');
        input.classList.remove('error');
        hasWarning = true;
      } else {
        warning.style.display = 'none';
        input.classList.remove('warning', 'error');
      }
    } else {
      warning.style.display = 'none';
      input.classList.remove('warning', 'error');
    }

    return { hasError, hasWarning };
  }

  /**
   * Validate all size inputs and their relationships
   */
  function validate() {
    const minValue = parseInt(minSizeInput.value, 10);
    const maxValue = parseInt(maxSizeInput.value, 10);

    // Validate individual inputs
    const minResult = validateSizeInput(minSizeInput, minSizeWarning, minValue);
    const maxResult = validateSizeInput(maxSizeInput, maxSizeWarning, maxValue);

    // Check if min >= max (range validation)
    if (minSizeInput.value && maxSizeInput.value && !isNaN(minValue) && !isNaN(maxValue)) {
      if (minValue >= maxValue) {
        rangeError.style.display = 'block';
        minSizeInput.classList.add('error');
        maxSizeInput.classList.add('error');
        return false; // Validation failed
      } else {
        rangeError.style.display = 'none';
        // Don't remove error class if individual validation failed
        if (!minResult.hasError) minSizeInput.classList.remove('error');
        if (!maxResult.hasError) maxSizeInput.classList.remove('error');
      }
    } else {
      rangeError.style.display = 'none';
    }

    // Return true if no errors (warnings are okay)
    return !minResult.hasError && !maxResult.hasError;
  }

  /**
   * Attach the validator to input events
   */
  function attach() {
    minSizeInput.addEventListener('input', validate);
    maxSizeInput.addEventListener('input', validate);
  }

  return {
    validate,
    attach,
    // Export constants for external use
    HARD_MAX_KB,
    RECOMMENDED_MAX_KB
  };
}

console.info('✅ validationUtils.js loaded - validation utilities ready');
