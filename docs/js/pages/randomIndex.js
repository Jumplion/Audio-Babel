/**
 * randomIndex.js
 * 
 * Generates random audio data for testing and demonstration.
 * Creates cryptographically random PCM data, encodes as base64 index,
 * and generates playback audio with metadata.
 */

import { getWasmModule } from '../core/wasmModule.js';
import { calculateDuration } from '../utils/audioIndex.js';
import { bytesToBase64Chunked, encodeBase64Url } from '../utils/utils.js';
import { handleError } from '../utils/errorHandler.js';

/**
 * Generate random audio data and send to API for processing
 * Creates random PCM data between configurable size limits,
 * encodes as base64 index, and generates result with metadata
 * @param {Function} handleJsonResponse - Callback for handling response
 * @param {Function} setLoading - Callback for loading state
 */
export async function generateAndSend(handleJsonResponse, setLoading) {
  try {
    setLoading(true);
    
    // Default values: 64KB and ~5MB
    const DEFAULT_MIN_KB = 64;
    const DEFAULT_MAX_KB = 5120; // 5MB
    const HARD_MAX_KB = 61440; // 60 MB - hard limit
    
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
    if (minKB > HARD_MAX_KB) {
      throw new Error(`Minimum size cannot exceed ${HARD_MAX_KB} KB (60 MB)`);
    }
    if (maxKB > HARD_MAX_KB) {
      throw new Error(`Maximum size cannot exceed ${HARD_MAX_KB} KB (60 MB)`);
    }
    
    // Convert to bytes (must be even number for 16-bit samples)
    const MIN_SIZE = Math.floor((minKB * 1024) / 2) * 2;
    const MAX_SIZE = Math.floor((maxKB * 1024) / 2) * 2;
    
    // Generate random size between custom or default range
    const size = Math.floor(Math.random() * ((MAX_SIZE - MIN_SIZE) / 2 + 1)) * 2 + MIN_SIZE;
    
    // Generate random bytes (PCM data)
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
    
    // Encode random PCM bytes as URL-safe base64 (this IS the user-facing index)
    const audioIndex = encodeBase64Url(randomBytes);
    
    // Calculate duration
    const duration = calculateDuration(randomBytes.length, 44100, 16, 1);
    
    // Create result object
    const result = {
      indexBase64: audioIndex,
      metadata: {
        genre: 'random',
        artist: 'generated',
        album: `${duration.toFixed(2)}s`,
        track: `${(audioIndex.length / 1024).toFixed(2)} KB`,
        cover: ''
      },
      sampleRate: 44100,
      numChannels: 1,
      dataSize: randomBytes.length,
      duration: duration
    };
    
    // Generate WAV for playback
    const wasm = await getWasmModule();
    const wavBlob = wasm.samplesToWav(randomBytes, 44100, 16, 1);
    const wavArrayBuffer = await wavBlob.arrayBuffer();
    const wavBytes = new Uint8Array(wavArrayBuffer);
    
    // Convert to base64 for audio player using shared utility
    result.wavBase64 = bytesToBase64Chunked(wavBytes);
    
    await handleJsonResponse(result, result.indexBase64);
  } catch (error) {
    handleError('randomIndex.js:generateAndSend', error, error.message);
  } finally {
    setLoading(false);
  }
}

