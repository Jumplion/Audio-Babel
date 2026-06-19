/**
 * wasmModule.js
 * 
 * Shared singleton WASM module instance.
 * Eliminates duplication of WASM initialization code across multiple files.
 * 
 * Usage:
 *   import { getWasmModule } from './wasmModule.js';
 *   const wasm = await getWasmModule();
 *   // Use wasm instance...
 */

import IndexWasm from './indexWasm.js';

let wasmInstance = null;

/**
 * Get the shared WASM module instance (lazy-loaded singleton)
 * Initializes on first call, returns cached instance on subsequent calls
 * @returns {Promise<IndexWasm>} Initialized WASM module instance
 */
export async function getWasmModule() {
    if (!wasmInstance) {
        wasmInstance = new IndexWasm();
        await wasmInstance.initialize();
    }
    return wasmInstance;
}

console.info('✅ wasmModule.js loaded - shared WASM singleton ready');
