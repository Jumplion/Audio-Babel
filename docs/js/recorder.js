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
 * Format duration in milliseconds to MM:SS
 * @param {number} ms - Duration in milliseconds
 * @returns {string} Formatted duration string
 */
export function formatDuration(ms) {
    const totalSec = Math.floor(ms / 1000);
    const mm = String(Math.floor(totalSec / 60)).padStart(2, '0');
    const ss = String(totalSec % 60).padStart(2, '0');
    return `${mm}:${ss}`;
}

/**
 * Create an audio recorder with recording controls
 * @param {Object} config - Configuration object
 * @param {HTMLAudioElement} config.recordPlayer - Audio element for playback
 * @param {HTMLElement} config.recordStatus - Status text element
 * @param {HTMLElement} config.recordDurationEl - Duration display element
 * @param {HTMLButtonElement} config.uploadRecording - Upload button
 * @param {Function} config.setLoading - Loading state callback
 * @param {Function} config.handleJsonResponse - Response handler callback
 * @returns {Object} Recorder interface
 */
export function createRecorder({ recordPlayer, recordStatus, recordDurationEl, uploadRecording, setLoading, handleJsonResponse }) {
  let mediaRecorder = null;
  let recordedChunks = [];
  let recordedBlob = null;
  let recordStartTime = 0;
  let recordTimerId = null;

  async function ensureMediaRecorder() {
    if (mediaRecorder) return mediaRecorder;
    
    if (!navigator.mediaDevices?.getUserMedia) {
      throw new Error('Media devices API not available');
    }
    
    const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    mediaRecorder = new MediaRecorder(stream);
    
    mediaRecorder.ondataavailable = (e) => {
      if (e.data?.size > 0) {
        recordedChunks.push(e.data);
      }
    };
    
    mediaRecorder.onstop = () => {
      recordedBlob = new Blob(recordedChunks, { type: 'audio/webm' });
      recordedChunks = [];
      
      const url = URL.createObjectURL(recordedBlob);
      if (recordPlayer) recordPlayer.src = url;
      if (uploadRecording) uploadRecording.disabled = false;
      if (recordStatus) recordStatus.textContent = 'Recorded';
      
      if (recordTimerId) {
        clearInterval(recordTimerId);
        recordTimerId = null;
      }
    };
    
    return mediaRecorder;
  }

  async function hasInputDevice() {
    try {
      if (!navigator.mediaDevices?.enumerateDevices) {
        return false;
      }
      const devices = await navigator.mediaDevices.enumerateDevices();
      return devices.some((device) => device?.kind === 'audioinput');
    } catch (error) {
      console.error('Error checking for input devices:', error);
      return false;
    }
  }

  async function startRecording() {
    setLoading(true);
    recordStatus.textContent = 'Recording...';
    const mr = await ensureMediaRecorder();
    recordedChunks = [];
    recordedBlob = null;
    if (uploadRecording) uploadRecording.disabled = true;
    mr.start();
    recordStartTime = Date.now();
    if (recordDurationEl) recordDurationEl.textContent = '00:00';
    recordTimerId = setInterval(() => {
      const elapsed = Date.now() - recordStartTime;
      if (recordDurationEl) recordDurationEl.textContent = formatDuration(elapsed);
    }, 250);
    setLoading(false);
  }

  function stopRecording() {
    setLoading(true);
    if (mediaRecorder && mediaRecorder.state === 'recording') mediaRecorder.stop();
    if (recordDurationEl) recordDurationEl.textContent = formatDuration(Date.now() - recordStartTime);
    setLoading(false);
  }

  /**
   * Helper function to write a string to a DataView
   * @param {DataView} view - The DataView to write to
   * @param {number} offset - The offset to start writing at
   * @param {string} string - The string to write
   */
  function writeString(view, offset, string) {
    for (let i = 0; i < string.length; i++) {
      view.setUint8(offset + i, string.charCodeAt(i));
    }
  }

  /**
   * Convert a WebM blob to WAV format.
   * @param {Blob} webmBlob - The WebM audio blob
   * @returns {Promise<Blob>} A promise that resolves to a WAV blob
   */
  async function convertWebMToWav(webmBlob) {
    const audioContext = new (window.AudioContext || window.webkitAudioContext)();
    const arrayBuffer = await webmBlob.arrayBuffer();
    const audioBuffer = await audioContext.decodeAudioData(arrayBuffer);

    // Extract PCM data
    const numChannels = audioBuffer.numberOfChannels;
    const sampleRate = audioBuffer.sampleRate;
    const length = audioBuffer.length * numChannels * 2; // 16-bit samples

    // Create WAV file structure
    const wavBuffer = new ArrayBuffer(44 + length);
    const view = new DataView(wavBuffer);

    // Write RIFF header
    writeString(view, 0, 'RIFF');
    view.setUint32(4, 36 + length, true);
    writeString(view, 8, 'WAVE');

    // Write fmt chunk
    writeString(view, 12, 'fmt ');
    view.setUint32(16, 16, true); // fmt chunk size
    view.setUint16(20, 1, true); // PCM format
    view.setUint16(22, numChannels, true);
    view.setUint32(24, sampleRate, true);
    view.setUint32(28, sampleRate * numChannels * 2, true); // byte rate
    view.setUint16(32, numChannels * 2, true); // block align
    view.setUint16(34, 16, true); // bits per sample

    // Write data chunk
    writeString(view, 36, 'data');
    view.setUint32(40, length, true);

    // Interleave and write PCM data
    let offset = 44;
    for (let i = 0; i < audioBuffer.length; i++) {
      for (let channel = 0; channel < numChannels; channel++) {
        const sample = Math.max(-1, Math.min(1, audioBuffer.getChannelData(channel)[i]));
        view.setInt16(offset, sample < 0 ? sample * 0x8000 : sample * 0x7FFF, true);
        offset += 2;
      }
    }

    return new Blob([wavBuffer], { type: 'audio/wav' });
  }

  async function uploadRecorded() {
    if (!recordedBlob) throw new Error('No recording available');
    try {
      setLoading(true);
      
      // Convert WebM to WAV format before processing
      const wavBlob = await convertWebMToWav(recordedBlob);
      const file = new File([wavBlob], 'recording.wav', { type: 'audio/wav' });
      
      // Use WASM for sample-based format (only format supported)
      const wasm = await getWasmModule();
      
      // Read WAV file
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
          genre: 'recorded',
          artist: 'microphone',
          album: `${duration.toFixed(2)}s`,
          track: `${(sampleBase64.length / 1024).toFixed(2)} KB`,
          cover: '' // No cover for sample-based
        },
        sampleRate: sampleRate,
        numChannels: numChannels,
        dataSize: pcmData.length,
        duration: duration
      };
      
      // Generate WAV for playback
      const wavBlobOutput = wasm.samplesToWav(pcmData, sampleRate, 16, numChannels);
      const wavArrayBuffer = await wavBlobOutput.arrayBuffer();
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
    } finally {
      setLoading(false);
    }
  }

  return { startRecording, stopRecording, uploadRecorded, ensureMediaRecorder, hasInputDevice };
}
