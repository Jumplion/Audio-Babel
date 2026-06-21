/**
 * errorHandler.js
 * 
 * Centralized error handling utilities for consistent user-facing error messages
 * and developer logging across the application.
 */

/**
 * Error severity levels
 */
export const ErrorLevel = {
  INFO: 'info',
  WARNING: 'warning',
  ERROR: 'error'
};

/**
 * Show a user-facing error message
 * @param {string} message - User-friendly error message
 * @param {ErrorLevel} level - Error severity level (default: ERROR)
 */
export function showError(message, level = ErrorLevel.ERROR) {
  // For now, use alert for simplicity
  // Could be enhanced with a custom modal or toast notification
  const prefix = level === ErrorLevel.WARNING ? '⚠ Warning: ' : level === ErrorLevel.INFO ? 'ℹ Info: ' : '❌ Error: ';
  alert(prefix + message);
}

/**
 * Log an error for developers
 * Logs to console with full error details including stack trace
 * @param {string} context - Context where error occurred (e.g., 'browse.js:enterRoom')
 * @param {Error|string} error - Error object or error message
 * @param {Object} additionalInfo - Additional debug information
 */
export function logError(context, error, additionalInfo = {}) {
  console.error(`[${context}]`, error);
  if (Object.keys(additionalInfo).length > 0) {
    console.error('Additional info:', additionalInfo);
  }
}

/**
 * Handle an error by logging it and showing a user-friendly message
 * Combines showError and logError for convenience
 * @param {string} context - Context where error occurred
 * @param {Error|string} error - Error object or message
 * @param {string} userMessage - User-friendly message (defaults to error message)
 * @param {ErrorLevel} level - Error severity level
 */
export function handleError(context, error, userMessage = null, level = ErrorLevel.ERROR) {
  // Log for developers
  logError(context, error);
  
  // Show user-friendly message
  const message = userMessage || (error?.message || String(error));
  showError(message, level);
}

/**
 * Handle a validation error (input validation failures)
 * @param {string} message - Validation error message
 */
export function showValidationError(message) {
  showError(message, ErrorLevel.WARNING);
}

console.info('✅ errorHandler.js loaded - error handling utilities ready');
