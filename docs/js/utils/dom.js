/**
 * Small DOM helper utilities shared across pages.
 */

/**
 * Escape HTML special characters to prevent XSS attacks
 * @param {string|number} s - String to escape
 * @returns {string} HTML-escaped string
 */
export function escapeHtml(s) {
  if (!s && s !== 0) return '';
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

/**
 * Trigger a file download from a blob
 * @param {Blob} blob - File content as blob
 * @param {string} filename - Name for downloaded file
 */
export function downloadBlob(blob, filename) {
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = filename;

  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);

  // Clean up blob URL
  setTimeout(() => URL.revokeObjectURL(url), 100);
}
