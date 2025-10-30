/**
 * search.js
 * 
 * Handles decoding and playback of base64 audio indexes.
 * Validates input, reconstructs audio from indexes, and provides
 * input filtering to ensure only valid base64 characters are entered.
 */

import { isValidBase64Url, calculateDuration } from '../utils/audioIndex.js';
import { getWasmModule } from '../core/wasmModule.js';
import { bytesToBase64Chunked, addIndexHeader, decodeBase64Url, encodeBase64Url } from '../utils/utils.js';
import { showValidationError, handleError } from '../utils/errorHandler.js';

/**
 * Generate audio from an index string
 * Validates the index, reconstructs PCM data, and generates playback audio
 * @param {HTMLElement} inputEl - Input element containing the index
 * @param {Function} handleJsonResponse - Callback for handling response
 * @param {Function} setLoading - Callback for loading state
 */
export async function generateFromIndex(inputEl, handleJsonResponse, setLoading) {
  const indexString = inputEl.value || '';
  
  // Validate index characters (A-Z, a-z, 0-9, -, _)
  if (!isValidBase64Url(indexString)) {
    showValidationError('Invalid characters in index. Only A-Z, a-z, 0-9, - and _ are allowed.');
    return;
  }
  
  try {
    setLoading(true);
    
    // Use WASM to reconstruct audio from index
    const wasm = await getWasmModule();
    
    // The user input is PCM-only (no header). We need to add a header to make it a valid audio index.
    let pcmBytes = decodeBase64Url(indexString);
    const bytesPerSample = 16 / 8; // 16-bit
    const numChannels = 1; // mono
    
    // Pad with zero byte if odd number of bytes (required for 16-bit audio)
    let pcmBase64 = indexString; // Will be updated if we pad
    if (pcmBytes.length % bytesPerSample !== 0) {
      console.log(`[search.js] Padding PCM data from ${pcmBytes.length} to ${pcmBytes.length + 1} bytes`);
      const paddedBytes = new Uint8Array(pcmBytes.length + 1);
      paddedBytes.set(pcmBytes, 0);
      paddedBytes[pcmBytes.length] = 0; // Pad with zero byte
      pcmBytes = paddedBytes;
      // Re-encode to base64 so addIndexHeader gets the padded data
      pcmBase64 = encodeBase64Url(pcmBytes);
    }
    
    const numFrames = pcmBytes.length / bytesPerSample / numChannels;
    
    // Add 13-byte header to create a valid audio index
    const fullIndex = addIndexHeader(pcmBase64, {
      numFrames: numFrames,
      sampleRate: 44100,
      bitDepth: 16,
      numChannels: 1
    });
    
    console.log('[search.js] Debug info:', {
      inputLength: indexString.length,
      pcmBytesLength: pcmBytes.length,
      numFrames: numFrames,
      fullIndexLength: fullIndex.length,
      fullIndexStart: fullIndex.substring(0, 20)
    });
    
    // Reconstruct audio from the full index (with header)
    const pcmData = wasm.reconstructAudioFromIndex(fullIndex);
    
    // Calculate duration
    const duration = calculateDuration(pcmData.length, 44100, 16, 1);
    
    // Create result object
    const result = {
      indexBase64: indexString,
      metadata: {
        genre: 'decoded',
        artist: 'search',
        album: `${duration.toFixed(2)}s`,
        track: `${(indexString.length / 1024).toFixed(2)} KB`,
        cover: ''
      },
      sampleRate: 44100,
      numChannels: 1,
      dataSize: pcmData.length,
      duration: duration
    };
    
    // Generate WAV for playback
    const wavBlob = wasm.samplesToWav(pcmData, 44100, 16, 1);
    const wavArrayBuffer = await wavBlob.arrayBuffer();
    const wavBytes = new Uint8Array(wavArrayBuffer);
    
    // Convert to base64 for audio player using shared utility
    result.wavBase64 = bytesToBase64Chunked(wavBytes);
    
    await handleJsonResponse(result, indexString);
  } catch (error) {
    handleError('search.js:generateFromIndex', error, error.message);
  } finally {
    setLoading(false);
  }
}

/**
 * Filter a string to only contain valid base64 URL-safe characters
 * @param {string} text - Text to filter
 * @returns {string} Filtered text containing only A-Z, a-z, 0-9, -, _
 */
function filterToValidChars(text) {
  const allowed = /[A-Za-z0-9_-]/;
  return text.split('').filter((c) => allowed.test(c)).join('');
}

/**
 * Auto-resize a textarea to fit its content
 * @param {HTMLTextAreaElement} element - Textarea element to resize
 */
function autosizeTextarea(element) {
  try {
    element.style.height = 'auto';
    const height = element.scrollHeight;
    element.style.height = Math.max(24, height) + 'px';
  } catch (e) {
    // Silently ignore errors
  }
}

/**
 * Handle paste event to filter invalid characters
 * @param {ClipboardEvent} e - Paste event
 * @param {HTMLInputElement|HTMLTextAreaElement} inputEl - Input element
 */
function handlePaste(e, inputEl) {
  try {
    const text = (e.clipboardData || window.clipboardData).getData('text') || '';
    const filtered = filterToValidChars(text);
    
    if (filtered !== text) {
      e.preventDefault();
      
      // Insert filtered text at caret position
      const start = inputEl.selectionStart || 0;
      const end = inputEl.selectionEnd || 0;
      const currentValue = inputEl.value || '';
      
      inputEl.value = currentValue.slice(0, start) + filtered + currentValue.slice(end);
      
      const newPosition = start + filtered.length;
      inputEl.setSelectionRange(newPosition, newPosition);
      autosizeTextarea(inputEl);
    }
  } catch (err) {
    // Fallback: allow default paste behavior
  }
}

/**
 * Handle input event to sanitize programmatically inserted text
 * @param {HTMLInputElement|HTMLTextAreaElement} inputEl - Input element
 */
function handleInput(inputEl) {
  const value = inputEl.value || '';
  const filtered = filterToValidChars(value);
  
  if (filtered !== value) {
    const cursorPos = inputEl.selectionStart || filtered.length;
    inputEl.value = filtered;
    
    // Restore cursor position (adjust by 1 if character was removed)
    const newPos = Math.max(0, cursorPos - 1);
    inputEl.setSelectionRange(newPos, newPos);
  }
  
  autosizeTextarea(inputEl);
}

/**
 * Attach input filter to prevent invalid characters in base64 URL-safe input
 * Filters input to only allow A-Z, a-z, 0-9, -, _
 * Also auto-resizes textarea elements to fit content
 * @param {HTMLInputElement|HTMLTextAreaElement} inputEl - Input element to filter
 */
export function attachSearchInputFilter(inputEl) {
  if (!inputEl) return;
  
  const allowed = /[A-Za-z0-9_-]/;
  
  // Prevent invalid keypress
  inputEl.addEventListener('keypress', (e) => {
    const char = String.fromCharCode(e.charCode || e.which || 0);
    if (!allowed.test(char)) {
      e.preventDefault();
    }
  });
  
  // Sanitize pasted content
  inputEl.addEventListener('paste', (e) => handlePaste(e, inputEl));
  
  // Sanitize programmatic input (drag/drop, etc.)
  inputEl.addEventListener('input', () => handleInput(inputEl));
  
  // Set initial size
  autosizeTextarea(inputEl);
}
