/**
 * main.js
 * 
 * Main entry point for page-specific functionality.
 * Coordinates page initialization, event handlers, and UI state management
 * for random generation, search, file upload, and recording pages.
 */

import { generateAndSend } from './randomIndex.js';
import { generateFromIndex, attachSearchInputFilter } from './search.js';
import { uploadFile } from './fileUpload.js';
import { createRecorder } from './recorder.js';
import { handleJsonResponse } from './resultHandler.js';
import { createSizeValidator } from './validationUtils.js';
import { showValidationError, handleError } from './errorHandler.js';

// Note: No longer using fetch interception - using direct function calls instead

/**
 * Set loading state for the UI
 * Shows/hides spinner, updates status message, and disables/enables controls
 * @param {boolean} on - True to show loading state, false to hide
 */
function setLoading(on) {
  const spinner = document.getElementById('statusSpinner');
  const msg = document.getElementById('statusMsg');
  const regenBtn = document.getElementById('regenBtn');
  const doSearchGenerate = document.getElementById('doSearchGenerate');
  const doFileSearch = document.getElementById('doFileSearch');
  const recordStartStop = document.getElementById('recordStartStop');
  const uploadRecording = document.getElementById('uploadRecording');
  const controls = [regenBtn, doSearchGenerate, doFileSearch, recordStartStop, uploadRecording];
  const resultContainer = document.getElementById('resultContainer');
  if (on) {
    if (spinner) spinner.style.display = 'inline-block';
    if (msg) msg.textContent = 'Loading...';
    controls.forEach((c) => c && c.classList.add('disabled'));
    if (resultContainer) resultContainer.classList.add('skeleton');
  } else {
    if (spinner) spinner.style.display = 'none';
    if (msg) msg.textContent = 'Ready';
    controls.forEach((c) => c && c.classList.remove('disabled'));
    if (resultContainer) resultContainer.classList.remove('skeleton');
  }
}

document.addEventListener('DOMContentLoaded', () => {
  // Random page: Generate button and input validation
  const regenBtn = document.getElementById('regenBtn');
  if (regenBtn) {
    regenBtn.addEventListener('click', () => generateAndSend(handleJsonResponse, setLoading));
    
    // Set up input validation for random page size inputs
    const minSizeInput = document.getElementById('minSize');
    const maxSizeInput = document.getElementById('maxSize');
    const minSizeWarning = document.getElementById('minSizeWarning');
    const maxSizeWarning = document.getElementById('maxSizeWarning');
    const rangeError = document.getElementById('rangeError');
    
    if (minSizeInput && maxSizeInput && minSizeWarning && maxSizeWarning && rangeError) {
      const validator = createSizeValidator({
        minSizeInput,
        maxSizeInput,
        minSizeWarning,
        maxSizeWarning,
        rangeError
      });
      
      // Attach validator to input events
      validator.attach();
    }
  }

  // Search page: Input and generate button
  const searchInput = document.getElementById('searchInput');
  const doSearchGenerate = document.getElementById('doSearchGenerate');
  if (searchInput && doSearchGenerate) {
    // Attach filtering to prevent invalid characters
    attachSearchInputFilter(searchInput);
    
    // Generate button click handler
    doSearchGenerate.addEventListener('click', () => {
      generateFromIndex(searchInput, handleJsonResponse, setLoading);
    });
    
    // Enable/disable generate button based on input
    const updateSearchButtonState = () => {
      const value = searchInput.value.trim();
      if (value.length > 0) {
        doSearchGenerate.removeAttribute('disabled');
      } else {
        doSearchGenerate.setAttribute('disabled', '');
      }
    };
    searchInput.addEventListener('input', updateSearchButtonState);
    updateSearchButtonState(); // Set initial state
  }

  // File upload page: File input and upload button
  const fileInput = document.getElementById('fileInput');
  const doFileSearch = document.getElementById('doFileSearch');
  if (fileInput && doFileSearch) {
    doFileSearch.addEventListener('click', async () => {
      const file = fileInput.files?.[0];
      if (!file) {
        showValidationError('Please select a .wav file first');
        return;
      }
      await uploadFile(file, handleJsonResponse, setLoading);
    });
  }

  // Record page: Recording controls
  const recordStartStop = document.getElementById('recordStartStop');
  const uploadRecording = document.getElementById('uploadRecording');
  const recordPlayer = document.getElementById('recordPlayer');
  const recordStatus = document.getElementById('recordStatus');
  const recordDuration = document.getElementById('recordDuration');
  
  if (recordStartStop && recordPlayer && uploadRecording) {
    const recorder = createRecorder({
      recordPlayer,
      recordStatus,
      recordDurationEl: recordDuration,
      uploadRecording,
      setLoading,
      handleJsonResponse
    });
    
    let recording = false;
    
    recordStartStop.addEventListener('click', async () => {
      try {
        if (!recording) {
          const hasDevice = await recorder.hasInputDevice();
          if (!hasDevice) {
            showValidationError('No audio input devices were detected. Please plug in or enable a microphone and try again.');
            return;
          }
          await recorder.startRecording();
          recording = true;
          recordStartStop.textContent = 'Stop Recording';
        } else {
          recorder.stopRecording();
          recording = false;
          recordStartStop.textContent = 'Start Recording';
        }
      } catch (e) {
        handleError('main.js:recordStartStop', e, 'Recording failed: ' + e.message);
      }
    });

    uploadRecording.addEventListener('click', async () => {
      try {
        await recorder.uploadRecorded();
      } catch (e) {
        handleError('main.js:uploadRecording', e, 'Upload failed: ' + e.message);
      }
    });
  }
});
