import { bytesToBase64 } from './utils.js';
import { encodeBase64Url } from './audioIndex.js';
import { clientReconstruct } from './apiAdapter.js';

/**
 * Generate random audio data and send to API for processing
 * @param {HTMLElement} regenBtn - Button element (unused but kept for consistency)
 * @param {Function} handleJsonResponse - Callback for handling response
 * @param {Function} setLoading - Callback for loading state
 */
export async function generateAndSend(regenBtn, handleJsonResponse, setLoading) {
  try {
    setLoading(true);
    
    // Default values: 64KB and ~5MB
    const DEFAULT_MIN_KB = 64;
    const DEFAULT_MAX_KB = 5120; // 5MB
    
    // Get custom values from input fields if specified
    const minSizeInput = document.getElementById('minSize');
    const maxSizeInput = document.getElementById('maxSize');
    
    const minKB = minSizeInput?.value ? parseInt(minSizeInput.value, 10) : DEFAULT_MIN_KB;
    const maxKB = maxSizeInput?.value ? parseInt(maxSizeInput.value, 10) : DEFAULT_MAX_KB;
    
    // Validate min and max
    if (isNaN(minKB) || minKB < 1) {
      throw new Error('Minimum size must be at least 1 KB');
    }
    if (isNaN(maxKB) || maxKB < 1) {
      throw new Error('Maximum size must be at least 1 KB');
    }
    if (minKB >= maxKB) {
      throw new Error('Minimum size must be less than maximum size');
    }
    
    // Convert to bytes (no upper limit enforced)
    const MIN_SIZE = minKB * 1024;
    const MAX_SIZE = maxKB * 1024;
    
    // Generate random size between custom or default range
    const size = Math.floor(Math.random() * (MAX_SIZE - MIN_SIZE + 1)) + MIN_SIZE;
    
    // Generate random bytes
    const randomBytes = new Uint8Array(size);
    if (window.crypto?.getRandomValues) {
      // Use secure random generation in chunks
      const CHUNK_SIZE = 65536;
      for (let i = 0; i < size; i += CHUNK_SIZE) {
        const end = Math.min(i + CHUNK_SIZE, size);
        window.crypto.getRandomValues(randomBytes.subarray(i, end));
      }
    } else {
      // Fallback to Math.random (less secure)
      console.warn('Using Math.random fallback - cryptographically insecure');
      for (let i = 0; i < size; i++) {
        randomBytes[i] = Math.floor(Math.random() * 256);
      }
    }
    
    // Convert to URL-safe base64 for the index
    const indexString = encodeBase64Url(randomBytes);
    
    // Convert to standard base64 for API
    const b64 = bytesToBase64(randomBytes);
    
    // Use client-side adapter instead of fetch
    const result = await clientReconstruct(b64, 'base64');
    await handleJsonResponse(result, indexString);
  } catch (error) {
    console.error('Error generating random index:', error);
    alert('Error: ' + error.message);
  } finally {
    setLoading(false);
  }
}
