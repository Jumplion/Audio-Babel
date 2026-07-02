/**
 * Centralized error handling: consistent user-facing messages plus developer
 * logging.
 */

export const ErrorLevel = {
  INFO: 'info',
  WARNING: 'warning',
  ERROR: 'error',
};

// Shows a user-facing error message.
export function showError(message, level = ErrorLevel.ERROR) {
  // For now, use alert for simplicity — could be a custom modal/toast later.
  const prefix =
    level === ErrorLevel.WARNING
      ? '⚠ Warning: '
      : level === ErrorLevel.INFO
        ? 'ℹ Info: '
        : '❌ Error: ';
  alert(prefix + message);
}

// Logs an error (with any extra debug info) to the console.
export function logError(context, error, additionalInfo = {}) {
  console.error(`[${context}]`, error);
  if (Object.keys(additionalInfo).length > 0) {
    console.error('Additional info:', additionalInfo);
  }
}

// Logs the error and shows a user-friendly message in one call.
export function handleError(context, error, userMessage = null, level = ErrorLevel.ERROR) {
  logError(context, error);

  const message = userMessage || error?.message || String(error);
  showError(message, level);
}

export function showValidationError(message) {
  showError(message, ErrorLevel.WARNING);
}

console.info('✅ errorHandler.js loaded - error handling utilities ready');
