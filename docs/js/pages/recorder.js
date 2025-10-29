import { getWasmModule } from '../core/wasmModule.js';
import { calculateDuration } from '../utils/audioIndex.js';
import { parseWavFile, convertWebMToWav } from '../utils/wavUtils.js';
import { bytesToBase64Chunked, encodeBase64Url } from '../utils/utils.js';

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

  /**
   * Ensure MediaRecorder is initialized
   * Creates and configures MediaRecorder on first call, returns cached instance on subsequent calls
   * @returns {Promise<MediaRecorder>} Initialized MediaRecorder instance
   * @throws {Error} If Media devices API is not available
   * @private
   */
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

  /**
   * Check if an audio input device is available
   * @returns {Promise<boolean>} True if audio input device exists
   * @private
   */
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

  /**
   * Start recording audio from the microphone
   * Initializes MediaRecorder, starts recording, and begins duration timer
   * @returns {Promise<void>}
   * @private
   */
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

  /**
   * Stop the current recording
   * Stops MediaRecorder and duration timer, updates final duration display
   * @private
   */
  function stopRecording() {
    setLoading(true);
    if (mediaRecorder && mediaRecorder.state === 'recording') mediaRecorder.stop();
    if (recordDurationEl) recordDurationEl.textContent = formatDuration(Date.now() - recordStartTime);
    setLoading(false);
  }

  /**
   * Upload the recorded audio and generate index
   * Converts WebM recording to WAV, generates base64 index, and displays result
   * @returns {Promise<void>}
   * @throws {Error} If no recording available
   * @private
   */
  async function uploadRecorded() {
    if (!recordedBlob) throw new Error('No recording available');
    try {
      setLoading(true);
      
      // Convert WebM to WAV format before processing
      const wavBlob = await convertWebMToWav(recordedBlob);
      const file = new File([wavBlob], 'recording.wav', { type: 'audio/wav' });
      
      // Use WASM to generate audio index
      const wasm = await getWasmModule();
      
      // Read WAV file and parse using shared utility
      const arrayBuffer = await file.arrayBuffer();
      const { pcmData, sampleRate, numChannels } = parseWavFile(arrayBuffer);
      
      // Encode PCM data as URL-safe base64 (this IS the user-facing index)
      const audioIndex = encodeBase64Url(pcmData);
      
      // Calculate duration
      const duration = calculateDuration(pcmData.length, sampleRate, 16, numChannels);
      
      // Create result object
      const result = {
        indexBase64: audioIndex,
        metadata: {
          genre: 'recorded',
          artist: 'microphone',
          album: `${duration.toFixed(2)}s`,
          track: `${(audioIndex.length / 1024).toFixed(2)} KB`,
          cover: ''
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
