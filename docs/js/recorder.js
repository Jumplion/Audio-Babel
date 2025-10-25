import AudioIndexWASM from './audioIndexWasm.js';
import { calculateDuration } from './audioIndex.js';
import { parseWavFile, convertWebMToWav } from './wavUtils.js';
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

  async function uploadRecorded() {
    if (!recordedBlob) throw new Error('No recording available');
    try {
      setLoading(true);
      
      // Convert WebM to WAV format before processing
      const wavBlob = await convertWebMToWav(recordedBlob);
      const file = new File([wavBlob], 'recording.wav', { type: 'audio/wav' });
      
      // Use WASM for sample-based format (only format supported)
      const wasm = await getWasmModule();
      
      // Read WAV file and parse using shared utility
      const arrayBuffer = await file.arrayBuffer();
      const { pcmData, sampleRate, numChannels } = parseWavFile(arrayBuffer);
      
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
      
      // Convert to base64 for audio player using shared utility
      result.wavBase64 = bytesToBase64Chunked(wavBytes);
      
      await handleJsonResponse(result, result.indexBase64);
    } finally {
      setLoading(false);
    }
  }

  return { startRecording, stopRecording, uploadRecorded, ensureMediaRecorder, hasInputDevice };
}
