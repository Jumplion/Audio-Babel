import { clientSearchByFile } from './apiAdapter.js';

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
    // Convert blob to File object for clientSearchByFile
    const file = new File([recordedBlob], 'recording.webm', { type: recordedBlob.type });
    try {
      setLoading(true);
      // Use client-side adapter instead of fetch
      const result = await clientSearchByFile(file);
      await handleJsonResponse(result, result.indexBase64);
    } finally {
      setLoading(false);
    }
  }

  return { startRecording, stopRecording, uploadRecorded, ensureMediaRecorder, hasInputDevice };
}
