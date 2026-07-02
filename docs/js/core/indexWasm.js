/**
 * JavaScript wrapper for the Audio Index WASM module.
 */

// True if a WASM string-returning call signaled failure by returning a JSON
// error object ({"error":"..."}) instead of the expected value.
export function isWasmError(result) {
  return typeof result === 'string' && result.startsWith('{');
}

class IndexWasm {
  constructor() {
    this.module = null;
    this.initialized = false;
  }

  async initialize() {
    if (this.initialized) {
      return;
    }

    console.log('Loading Audio Index WASM module...');

    try {
      // Path relative to docs/js/core/ -> ../../wasm/index.js
      const IndexModule = (await import('../../wasm/index.js')).default;

      // Bump whenever the WASM module is rebuilt, to bust the browser cache.
      const wasmVersion = '16';

      const isLocalhost =
        window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1';
      const wasmBasePath = isLocalhost
        ? '/wasm/' // Absolute path from server root (docs/)
        : `${window.location.origin}/Audio-Babel/wasm/`; // GitHub Pages includes the repo name

      console.log(`Environment: ${isLocalhost ? 'localhost' : 'production'}`);
      console.log(`WASM base path: ${wasmBasePath}`);

      this.module = await IndexModule({
        locateFile: (path) => {
          if (path.endsWith('.wasm')) {
            const fullPath = `${wasmBasePath}${path}?v=${wasmVersion}`;
            console.log(`Loading WASM from: ${fullPath}`);
            return fullPath;
          }
          return path;
        },
      });

      this.initialized = true;
      console.log('✓ Audio Index WASM module loaded successfully');
      console.log(
        'Available functions:',
        Object.keys(this.module).filter((k) => typeof this.module[k] === 'function')
      );
    } catch (error) {
      console.error('Failed to load WASM module:', error);
      throw error;
    }
  }

  _ensureInitialized() {
    if (!this.initialized) {
      throw new Error('WASM module not initialized. Call initialize() first.');
    }
  }

  // Encodes raw PCM bytes into a bijective base64 index string (no header).
  encodeIndexFromPcm(pcmBytes, sampleRate, bitDepth, numChannels) {
    this._ensureInitialized();

    const result = this.module.encodeIndex(pcmBytes, sampleRate, bitDepth, numChannels);
    if (!result || isWasmError(result)) {
      const errResult = result ? JSON.parse(result) : {};
      throw new Error(errResult.error || 'Failed to encode index from PCM data');
    }
    return result;
  }
}

export default IndexWasm;

// Also make available globally for non-module scripts
if (typeof window !== 'undefined') {
  window.IndexWasm = IndexWasm;
}

console.log('✅ indexWasm.js loaded - WASM wrapper ready');
