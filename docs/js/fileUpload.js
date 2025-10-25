import AudioIndexWASM from './audioIndexWasm.js';
import { calculateDuration } from './audioIndex.js';

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
 * Upload a WAV file for indexing
 * @param {File} file - WAV file to upload
 * @param {Function} handleJsonResponse - Callback for handling response
 * @param {Function} setLoading - Callback for loading state
 */
export async function uploadFile(file, handleJsonResponse, setLoading) {
  if (!file) {
    throw new Error('No file provided');
  }
  
  try {
    setLoading(true);
    
    // Use WASM for sample-based format
    const wasm = await getWasmModule();
    
    // Read file as ArrayBuffer
    const arrayBuffer = await file.arrayBuffer();
    const view = new DataView(arrayBuffer);
    
    // Parse WAV header to find data chunk
    let offset = 12; // Skip RIFF/WAVE header
    let pcmData = null;
    let sampleRate = 44100;
    let numChannels = 1;
    
    while (offset < view.byteLength - 8) {
      const chunkId = String.fromCharCode(
        view.getUint8(offset),
        view.getUint8(offset + 1),
        view.getUint8(offset + 2),
        view.getUint8(offset + 3)
      );
      const chunkSize = view.getUint32(offset + 4, true);
      offset += 8;
      
      if (chunkId === 'fmt ') {
        sampleRate = view.getUint32(offset + 4, true);
        numChannels = view.getUint16(offset + 2, true);
        offset += chunkSize;
      } else if (chunkId === 'data') {
        pcmData = new Uint8Array(arrayBuffer, offset, chunkSize);
        break;
      } else {
        offset += chunkSize + (chunkSize & 1);
      }
    }
    
    if (!pcmData) {
      throw new Error('No data chunk found in WAV file');
    }
    
    // Encode to sample-based base64 using WASM
    const sampleBase64 = wasm.encodeToSampleBase64(pcmData, sampleRate, numChannels);
    
    // Calculate duration
    const duration = calculateDuration(pcmData.length, sampleRate, 16, numChannels);
    
    // Create result object
    const result = {
      indexBase64: sampleBase64,
      metadata: {
        genre: 'uploaded',
        artist: file.name,
        album: `${duration.toFixed(2)}s`,
        track: `${(sampleBase64.length / 1024).toFixed(2)} KB`,
        cover: ''
      },
      sampleRate: sampleRate,
      numChannels: numChannels,
      dataSize: pcmData.length,
      duration: duration
    };
    
    // Generate WAV for playback
    const wavBlob = wasm.samplesToWav(pcmData, sampleRate, 16, numChannels);
    const wavArrayBuffer = await wavBlob.arrayBuffer();
    const wavBytes = new Uint8Array(wavArrayBuffer);
    
    // Convert to base64 for audio player
    let wavBase64 = '';
    const chunkSize = 0x8000;
    for (let i = 0; i < wavBytes.length; i += chunkSize) {
      const chunk = wavBytes.subarray(i, i + chunkSize);
      wavBase64 += String.fromCharCode.apply(null, chunk);
    }
    result.wavBase64 = btoa(wavBase64);
    
    await handleJsonResponse(result, result.indexBase64);
  } catch (error) {
    console.error(error);
    throw error; // Re-throw to let caller handle
  } finally {
    setLoading(false);
  }
}

