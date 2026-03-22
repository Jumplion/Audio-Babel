/**
 * fileUpload.js
 * 
 * Handles WAV file uploads for audio indexing.
 * Parses WAV files, encodes PCM data as base64 index,
 * and generates playback audio with metadata.
 */

import { getWasmModule } from '../core/wasmModule.js';
import { buildResult, DEFAULT_BIT_DEPTH } from '../utils/audioIndex.js';
import { parseWavFile } from '../utils/wavUtils.js';
import { bytesToBase64Chunked, encodeBase64Url } from '../utils/utils.js';

/**
 * Upload a WAV file for indexing
 * Parses the WAV file, generates a base64 audio index,
 * and creates a result object with metadata and playback audio
 * @param {File} file - WAV file to upload
 * @param {Function} handleJsonResponse - Callback for handling response
 * @param {Function} setLoading - Callback for loading state
 * @throws {Error} If no file provided or processing fails
 */
export async function uploadFile(file, handleJsonResponse, setLoading) {
  if (!file) {
    throw new Error('No file provided');
  }
  
  try {
    setLoading(true);
    
    // Use WASM to generate audio index
    const wasm = await getWasmModule();
    
    // Read file as ArrayBuffer and parse WAV
    const arrayBuffer = await file.arrayBuffer();
    const { pcmData, sampleRate, numChannels, bitDepth } = parseWavFile(arrayBuffer);
    
    // Encode PCM data as URL-safe base64 (this IS the user-facing index)
    const audioIndex = encodeBase64Url(pcmData);
    
    // Create result object
    const result = buildResult({ indexBase64: audioIndex, genre: 'uploaded', artist: file.name, pcmDataSize: pcmData.length, sampleRate, numChannels });
    
    // Generate WAV for playback
    const wavBlob = wasm.samplesToWav(pcmData, sampleRate, DEFAULT_BIT_DEPTH, numChannels);
    const wavArrayBuffer = await wavBlob.arrayBuffer();
    const wavBytes = new Uint8Array(wavArrayBuffer);
    
    // Convert to base64 for audio player using shared utility
    result.wavBase64 = bytesToBase64Chunked(wavBytes);
    
    await handleJsonResponse(result, result.indexBase64);
  } catch (error) {
    console.error(error);
    throw error; // Re-throw to let caller handle
  } finally {
    setLoading(false);
  }
}

