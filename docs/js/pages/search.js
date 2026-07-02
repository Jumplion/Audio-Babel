// Search page logic: reconstruct audio from a pasted index, generate a
// random index, or upload a WAV to derive its index. All three converge on
// buildResultForIndex. The index string is passed to/from WASM exactly as-is
// — no header is ever added or stripped.

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

async function renderIndex(indexString, handleJsonResponse, wavOptions) {
  const wasm = await getWasmModule();
  const result = await buildResultForIndex(wasm, indexString, wavOptions);
  await handleJsonResponse(result, indexString);
}

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

// Every alphabet-valid string decodes to a valid payload, so a random index
// never needs to go through the PCM->index encode path.
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

// Derives a WAV file's real index via WASM's encodeIndex and populates the
// index input with it. Does not reconstruct/display audio — the user
// triggers that themselves via the Reconstruct button. Resolves to the
// uploaded file's actual WAV properties (so the caller can sync the
// advanced-options dropdowns to it), or undefined if extraction failed.
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

function autosizeTextarea(element) {
  try {
    element.style.height = 'auto';
    const height = element.scrollHeight;
    element.style.height = Math.max(24, height) + 'px';
  } catch (e) {
    // Silently ignore errors
  }
}

function handlePaste(e, inputEl) {
  try {
    const text = (e.clipboardData || window.clipboardData).getData('text') || '';
    const filtered = filterToBase64UrlChars(text);

    if (filtered !== text) {
      e.preventDefault();

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

// Filters an input/textarea to base64url characters (A-Z, a-z, 0-9, -, _)
// on keypress, paste, and programmatic input, and keeps it autosized.
export function attachSearchInputFilter(inputEl) {
  if (!inputEl) return;

  inputEl.addEventListener('keypress', (e) => {
    const char = String.fromCharCode(e.charCode || e.which || 0);
    if (!VALID_BASE64_URL_CHAR.test(char)) {
      e.preventDefault();
    }
  });

  inputEl.addEventListener('paste', (e) => handlePaste(e, inputEl));
  inputEl.addEventListener('input', () => handleInput(inputEl));

  autosizeTextarea(inputEl);
}
