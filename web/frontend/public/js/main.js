import { generateAndSend } from './randomIndex.js';
import { generateFromIndex, attachSearchInputFilter } from './search.js';
import { uploadFile } from './fileUpload.js';
import { createRecorder } from './recorder.js';
import { handleJsonResponse } from './resultHandler.js';

// Note: No longer using fetch interception - using direct function calls instead

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
    regenBtn.addEventListener('click', () => generateAndSend(regenBtn, handleJsonResponse, setLoading));
    
    // Set up input validation for random page size inputs
    const minSizeInput = document.getElementById('minSize');
    const maxSizeInput = document.getElementById('maxSize');
    const minSizeWarning = document.getElementById('minSizeWarning');
    const maxSizeWarning = document.getElementById('maxSizeWarning');
    const rangeError = document.getElementById('rangeError');
    
    if (minSizeInput && maxSizeInput && minSizeWarning && maxSizeWarning && rangeError) {
      const RECOMMENDED_MAX_KB = 61440; // 60 MB
      const WARNING_THRESHOLD_KB = 102400; // 100 MB
      
      function validateInputs() {
        let hasWarning = false;
        let hasError = false;
        
        const minValue = parseInt(minSizeInput.value, 10);
        const maxValue = parseInt(maxSizeInput.value, 10);
        
        // Check min size warnings
        if (minSizeInput.value && !isNaN(minValue)) {
          if (minValue > WARNING_THRESHOLD_KB) {
            minSizeWarning.textContent = `⚠ Warning: ${minValue} KB is very large and may cause performance issues`;
            minSizeWarning.style.display = 'block';
            minSizeInput.classList.add('warning');
            hasWarning = true;
          } else if (minValue > RECOMMENDED_MAX_KB) {
            minSizeWarning.textContent = `⚠ Warning: ${minValue} KB exceeds recommended maximum (60 MB)`;
            minSizeWarning.style.display = 'block';
            minSizeInput.classList.add('warning');
            hasWarning = true;
          } else {
            minSizeWarning.style.display = 'none';
            minSizeInput.classList.remove('warning');
          }
        } else {
          minSizeWarning.style.display = 'none';
          minSizeInput.classList.remove('warning');
        }
        
        // Check max size warnings
        if (maxSizeInput.value && !isNaN(maxValue)) {
          if (maxValue > WARNING_THRESHOLD_KB) {
            maxSizeWarning.textContent = `⚠ Warning: ${maxValue} KB is very large and may cause performance issues`;
            maxSizeWarning.style.display = 'block';
            maxSizeInput.classList.add('warning');
            hasWarning = true;
          } else if (maxValue > RECOMMENDED_MAX_KB) {
            maxSizeWarning.textContent = `⚠ Warning: ${maxValue} KB exceeds recommended maximum (60 MB)`;
            maxSizeWarning.style.display = 'block';
            maxSizeInput.classList.add('warning');
            hasWarning = true;
          } else {
            maxSizeWarning.style.display = 'none';
            maxSizeInput.classList.remove('warning');
          }
        } else {
          maxSizeWarning.style.display = 'none';
          maxSizeInput.classList.remove('warning');
        }
        
        // Check if min >= max
        if (minSizeInput.value && maxSizeInput.value && !isNaN(minValue) && !isNaN(maxValue)) {
          if (minValue >= maxValue) {
            rangeError.style.display = 'block';
            minSizeInput.classList.add('error');
            maxSizeInput.classList.add('error');
            hasError = true;
          } else {
            rangeError.style.display = 'none';
            minSizeInput.classList.remove('error');
            maxSizeInput.classList.remove('error');
          }
        } else {
          rangeError.style.display = 'none';
          minSizeInput.classList.remove('error');
          maxSizeInput.classList.remove('error');
        }
      }
      
      minSizeInput.addEventListener('input', validateInputs);
      maxSizeInput.addEventListener('input', validateInputs);
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
      generateFromIndex(searchInput, doSearchGenerate, handleJsonResponse, setLoading);
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
        alert('Please select a .wav file first');
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
            alert('No audio input devices were detected. Please plug in or enable a microphone and try again.');
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
        console.error(e);
        alert('Recording failed: ' + e.message);
      }
    });

    uploadRecording.addEventListener('click', async () => {
      try {
        await recorder.uploadRecorded();
      } catch (e) {
        console.error(e);
        alert('Upload failed: ' + e.message);
      }
    });
  }
});
