import { generateAndSend } from './randomIndex.js';
import { generateFromIndex, attachBrowseInputFilter } from './browse.js';
import { uploadFile } from './fileUpload.js';
import { createRecorder } from './recorder.js';
import { handleJsonResponse } from './resultHandler.js';

function setActiveTabUI(tab) {
  // Only update tab UI when tab buttons exist (single-page layout)
  const tabRandom = document.getElementById('tabRandom');
  const tabFile = document.getElementById('tabFile');
  const tabRecord = document.getElementById('tabRecord');
  if (!tabRandom && !tabFile && !tabRecord) return;
  if (tabRandom) tabRandom.classList.remove('tab-active');
  if (tabFile) tabFile.classList.remove('tab-active');
  if (tabRecord) tabRecord.classList.remove('tab-active');
  if (tab === 'random' && tabRandom) tabRandom.classList.add('tab-active');
  if (tab === 'file' && tabFile) tabFile.classList.add('tab-active');
  if (tab === 'record' && tabRecord) tabRecord.classList.add('tab-active');
}

function setLoading(on) {
  const spinner = document.getElementById('statusSpinner');
  const msg = document.getElementById('statusMsg');
  const regenBtn = document.getElementById('regenBtn');
  const doBrowseGenerate = document.getElementById('doBrowseGenerate');
  const doFileSearch = document.getElementById('doFileSearch');
  const recordStartStop = document.getElementById('recordStartStop');
  const uploadRecording = document.getElementById('uploadRecording');
  const controls = [regenBtn, doBrowseGenerate, doFileSearch, recordStartStop, uploadRecording];
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
  const tabRandom = document.getElementById('tabRandom');
  const tabFile = document.getElementById('tabFile');
  const tabRecord = document.getElementById('tabRecord');
  const panelRandom = document.getElementById('panelRandom');
  const panelFile = document.getElementById('panelFile');
  const panelBrowse = document.getElementById('panelBrowse');
  const panelRecord = document.getElementById('panelRecord');
  const regenBtn = document.getElementById('regenBtn');
  const fileInput = document.getElementById('fileInput');
  const doFileSearch = document.getElementById('doFileSearch');
  const recordStartStop = document.getElementById('recordStartStop');
  const uploadRecording = document.getElementById('uploadRecording');
  const recordStatus = document.getElementById('recordStatus');
  const recordPlayer = document.getElementById('recordPlayer');
  const recordDuration = document.getElementById('recordDuration');

  // Helper functions that operate only if relevant DOM exists
  function showRandom() {
    if (panelRandom) panelRandom.style.display = '';
    if (panelBrowse) panelBrowse.style.display = 'none';
    if (panelFile) panelFile.style.display = 'none';
    if (panelRecord) panelRecord.style.display = 'none';
    setActiveTabUI('random');
  }
  function showFile() {
    if (panelRandom) panelRandom.style.display = 'none';
    if (panelBrowse) panelBrowse.style.display = 'none';
    if (panelFile) panelFile.style.display = '';
    if (panelRecord) panelRecord.style.display = 'none';
    setActiveTabUI('file');
  }
  function showBrowse() {
    if (panelRandom) panelRandom.style.display = 'none';
    if (panelBrowse) panelBrowse.style.display = '';
    if (panelFile) panelFile.style.display = 'none';
    if (panelRecord) panelRecord.style.display = 'none';
    setActiveTabUI('browse');
  }
  function showRecord() {
    if (panelRandom) panelRandom.style.display = 'none';
    if (panelFile) panelFile.style.display = 'none';
    if (panelRecord) panelRecord.style.display = '';
    setActiveTabUI('record');
  }

  // Wire tab click handlers only if present (single-page layout)
  if (tabRandom) tabRandom.addEventListener('click', showRandom);
  const tabBrowse = document.getElementById('tabBrowse');
  if (tabBrowse) tabBrowse.addEventListener('click', showBrowse);
  if (tabFile) tabFile.addEventListener('click', showFile);
  if (tabRecord) tabRecord.addEventListener('click', showRecord);

  // regen button (exists on random page or index)
  if (regenBtn) regenBtn.addEventListener('click', () => generateAndSend(regenBtn, handleJsonResponse, setLoading));

  // browse input handling (exists on browse page)
  const browseInput = document.getElementById('browseInput');
  if (browseInput) {
    // attach filtering to prevent invalid characters
    attachBrowseInputFilter(browseInput);
    const doBrowseGenerateBtn = document.getElementById('doBrowseGenerate');
    if (doBrowseGenerateBtn) {
      doBrowseGenerateBtn.addEventListener('click', () => generateFromIndex(browseInput, doBrowseGenerateBtn, handleJsonResponse, setLoading));
      // enable generate button only when input is non-empty
      const updateBrowseButtonState = () => {
        if (!doBrowseGenerateBtn) return;
        const v = (browseInput && browseInput.value) ? browseInput.value.trim() : '';
        if (v.length > 0) doBrowseGenerateBtn.removeAttribute('disabled');
        else doBrowseGenerateBtn.setAttribute('disabled', '');
      };
      browseInput.addEventListener('input', updateBrowseButtonState);
      // set initial state
      updateBrowseButtonState();
    }
  }

  // file upload handling (exists on fileSearch page)
  if (doFileSearch) {
    doFileSearch.addEventListener('click', async () => {
      const file = fileInput && fileInput.files && fileInput.files[0];
      if (!file) return alert('Please select a .wav file first');
      await uploadFile(file, handleJsonResponse, setLoading);
    });
  }

  // recorder handling (exists on record page)
  if (recordStartStop && recordPlayer && uploadRecording) {
    const recorder = createRecorder({ recordPlayer, recordStatus, recordDurationEl: recordDuration, uploadRecording, setLoading, handleJsonResponse });
    let recording = false;
    recordStartStop.addEventListener('click', async () => {
      try {
        if (!recording) {
          const hasDevice = await recorder.hasInputDevice();
          if (!hasDevice) return alert('No audio input devices were detected. Please plug in or enable a microphone and try again.');
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

  // default: if single-page panels are present show random, otherwise do nothing
  if (panelRandom || panelBrowse || panelFile || panelRecord) showRandom();
});
