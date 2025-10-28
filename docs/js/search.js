import { isValidBase64Url, calculateDuration } from './audioIndex.js';
import AudioIndexWASM from './audioIndexWasm.js';
import { bytesToBase64Chunked } from './utils.js';

// Initialize WASM module (lazy-loaded)
let wasmModule = null;
async function getWasmModule() {
    if (!wasmModule) {
        wasmModule = new AudioIndexWASM();
        await wasmModule.initialize();
    }
    return wasmModule;
}

/**
 * Generate audio from an index string
 * @param {HTMLElement} inputEl - Input element containing the index
 * @param {HTMLElement} btnEl - Button element (unused but kept for consistency)
 * @param {Function} handleJsonResponse - Callback for handling response
 * @param {Function} setLoading - Callback for loading state
 */
export async function generateFromIndex(inputEl, btnEl, handleJsonResponse, setLoading) {
  const indexString = inputEl.value || '';
  
  // Validate index characters (A-Z, a-z, 0-9, -, _)
  if (!isValidBase64Url(indexString)) {
    alert('Invalid characters in index. Only A-Z, a-z, 0-9, - and _ are allowed.');
    return;
  }
  
  try {
    setLoading(true);
    
    // Use WASM to reconstruct audio from index
    const wasm = await getWasmModule();
    
    // Reconstruct audio from the index
    const pcmData = wasm.reconstructAudioFromIndex(indexString);
    
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
    console.error(error);
    alert('Error: ' + error.message);
  } finally {
    setLoading(false);
  }
}

export function attachSearchInputFilter(inputEl) {
  if (!inputEl) return;
  const allowed = /[A-Za-z0-9_-]/;

  function autosize() {
    try {
      inputEl.style.height = 'auto';
      const h = inputEl.scrollHeight;
      inputEl.style.height = Math.max(24, h) + 'px';
    } catch (e) {
      /* ignore */
    }
  }

  // prevent invalid key input
  inputEl.addEventListener('keypress', (e) => {
    const ch = String.fromCharCode(e.charCode || e.which || 0);
    if (!allowed.test(ch)) e.preventDefault();
  });

  // sanitize pasted content
  inputEl.addEventListener('paste', (e) => {
    try {
      const txt = (e.clipboardData || window.clipboardData).getData('text') || '';
      const filtered = txt.split('').filter((c) => allowed.test(c)).join('');
      if (filtered !== txt) {
        e.preventDefault();
        // insert filtered text at caret
  const start = inputEl.selectionStart || 0;
  const end = inputEl.selectionEnd || 0;
  const v = inputEl.value || '';
  inputEl.value = v.slice(0, start) + filtered + v.slice(end);
  const pos = start + filtered.length;
  inputEl.setSelectionRange(pos, pos);
  autosize();
      }
    } catch (err) {
      // fallback: do nothing
    }
  });

  // sanitize programmatic input (e.g., drag/drop) on input event
  inputEl.addEventListener('input', () => {
    const v = inputEl.value || '';
    const filtered = v.split('').filter((c) => allowed.test(c)).join('');
    if (filtered !== v) {
      const pos = inputEl.selectionStart || filtered.length;
      inputEl.value = filtered;
      inputEl.setSelectionRange(Math.max(0, pos - 1), Math.max(0, pos - 1));
    }
    autosize();
  });
  // initial size
  autosize();
}
