/**
 * search.js
 *
 * Single consolidated module for the Search page: reconstruct audio from a
 * pasted index, generate a random index, or upload a WAV to derive its index.
 * All three actions converge on the same render pipeline (buildResultForIndex),
 * so genre/artist/album/track/position always come from the real C++/WASM
 * getMetadata/calculatePosition calls — never fabricated client-side.
 *
 * The index string is passed to/from WASM exactly as-is: no header is ever
 * prepended, appended, or stripped. The bijective base64 index IS the payload
 * encoding; WAV headers only exist on the materialized .wav Blob for playback.
 */

import { buildResultForIndex } from '../utils/resultBuilder.js';
import { isValidBase64Url, filterToBase64UrlChars } from '../utils/validationUtils.js';
import { getWasmModule } from '../core/wasmModule.js';
import { parseWavFile } from '../utils/wavUtils.js';
import { showValidationError, handleError } from '../utils/errorHandler.js';
import { ALPHABET as BASE64_URL_ALPHABET } from '../utils/base64.js';

const VALID_BASE64_URL_CHAR = /[A-Za-z0-9_-]/;

// Default random index length range (characters), chosen for a reasonable
// demo duration without configurable "advanced options".
const RANDOM_MIN_CHARS = 65536; // ~64 KB  → ~0.5s of audio
const RANDOM_MAX_CHARS = 131072; // ~128 KB → ~1.1s of audio

/**
 * Render an index string: fetch its real metadata/audio from WASM and hand
 * the result to the shared display handler.
 * @param {string} indexString - Bijective base64 index (no header)
 * @param {Function} handleJsonResponse - Callback for handling response
 * @param {Object} [wavOptions] - Output WAV format overrides (sampleRate, bitDepth, numChannels)
 */
async function renderIndex(indexString, handleJsonResponse, wavOptions) {
  const wasm = await getWasmModule();
  const result = await buildResultForIndex(wasm, indexString, wavOptions);
  await handleJsonResponse(result, indexString);
}

/**
 * Reconstruct audio from a user-entered index string.
 * @param {HTMLElement} inputEl - Input element containing the index
 * @param {Function} handleJsonResponse - Callback for handling response
 * @param {Function} setLoading - Callback for loading state
 * @param {Object} [wavOptions] - Output WAV format overrides (sampleRate, bitDepth, numChannels)
 */
export async function generateFromIndex(inputEl, handleJsonResponse, setLoading, wavOptions) {
  const indexString = inputEl.value || '';

  if (!isValidBase64Url(indexString)) {
    showValidationError('Invalid characters in index. Only A-Z, a-z, 0-9, - and _ are allowed.');
    return;
  }

  try {
    setLoading(true);
    await renderIndex(indexString, handleJsonResponse, wavOptions);
  } catch (error) {
    handleError('search.js:generateFromIndex', error, error.message);
  } finally {
    setLoading(false);
  }
}

/**
 * Generate a random valid index string and reconstruct its audio.
 * Per the index format, every alphabet-valid string decodes to a valid
 * payload — so a random index never needs to go through the PCM->index
 * encode path.
 * @param {Function} handleJsonResponse - Callback for handling response
 * @param {Function} setLoading - Callback for loading state
 * @param {HTMLTextAreaElement} [inputEl] - Index input to populate with the generated index
 * @param {Object} [wavOptions] - Output WAV format overrides (sampleRate, bitDepth, numChannels)
 */
export async function generateRandom(handleJsonResponse, setLoading, inputEl, wavOptions) {
  try {
    setLoading(true);

    const length =
      RANDOM_MIN_CHARS + Math.floor(Math.random() * (RANDOM_MAX_CHARS - RANDOM_MIN_CHARS + 1));
    const randomIndices = new Uint8Array(length);
    // crypto.getRandomValues caps out at 65536 bytes per call, but our
    // random indices can be up to 1 MB — fill it in chunks.
    const MAX_CRYPTO_BYTES = 65536;
    for (let offset = 0; offset < length; offset += MAX_CRYPTO_BYTES) {
      window.crypto.getRandomValues(randomIndices.subarray(offset, offset + MAX_CRYPTO_BYTES));
    }

    let indexString = '';
    for (let i = 0; i < length; i++) {
      indexString += BASE64_URL_ALPHABET[randomIndices[i] % BASE64_URL_ALPHABET.length];
    }

    await renderIndex(indexString, handleJsonResponse, wavOptions);

    if (inputEl) {
      inputEl.value = indexString;
      // Re-run the same filter/autosize/button-state listeners that fire on user input.
      inputEl.dispatchEvent(new Event('input'));
    }
  } catch (error) {
    handleError('search.js:generateRandom', error, error.message);
  } finally {
    setLoading(false);
  }
}

/**
 * Derive a WAV file's real index via WASM's encodeIndex and populate the
 * index input with it. Does not reconstruct/display audio — the user
 * triggers that themselves via the Reconstruct button.
 * @param {File} file - WAV file to derive the index from
 * @param {HTMLTextAreaElement} inputEl - Index input to populate
 * @param {Function} setLoading - Callback for loading state
 * @returns {Promise<{sampleRate: number, bitDepth: number, numChannels: number}|undefined>}
 *   The uploaded file's actual WAV properties, so the caller can sync the
 *   advanced-options dropdowns to it. Undefined if extraction failed.
 */
export async function extractIndexFromWav(file, inputEl, setLoading) {
  if (!file) {
    throw new Error('No file provided');
  }

  try {
    setLoading(true);

    const wasm = await getWasmModule();
    const arrayBuffer = await file.arrayBuffer();
    const { pcmData, sampleRate, numChannels, bitDepth } = parseWavFile(arrayBuffer);

    const indexString = wasm.encodeIndexFromPcm(pcmData, sampleRate, bitDepth, numChannels);

    inputEl.value = indexString;
    inputEl.dispatchEvent(new Event('input'));

    return { sampleRate, bitDepth, numChannels };
  } catch (error) {
    handleError('search.js:extractIndexFromWav', error, error.message);
  } finally {
    setLoading(false);
  }
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
    const filtered = filterToBase64UrlChars(text);

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
  const filtered = filterToBase64UrlChars(value);

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

  // Prevent invalid keypress
  inputEl.addEventListener('keypress', (e) => {
    const char = String.fromCharCode(e.charCode || e.which || 0);
    if (!VALID_BASE64_URL_CHAR.test(char)) {
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
