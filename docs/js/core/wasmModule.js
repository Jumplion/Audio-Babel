/**
 * Shared singleton WASM module instance, so every page pays initialization
 * cost only once.
 *
 * Usage:
 *   import { getWasmModule } from './wasmModule.js';
 *   const wasm = await getWasmModule();
 */

import IndexWasm from './indexWasm.js';

let wasmInstance = null;

export async function getWasmModule() {
  if (!wasmInstance) {
    wasmInstance = new IndexWasm();
    await wasmInstance.initialize();
  }
  return wasmInstance;
}

console.info('✅ wasmModule.js loaded - shared WASM singleton ready');
