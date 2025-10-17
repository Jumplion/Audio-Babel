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
    
    // Generate random size between 64KB and ~5MB
    const MIN_SIZE = 65536; // 64KB
    const MAX_SIZE = 5242880; // ~5MB
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
    console.error(error);
    alert('Error: ' + error.message);
  } finally {
    setLoading(false);
  }
}
