import { generateAndSend } from './randomIndex.js';
import { generateFromIndex, attachBrowseInputFilter } from './browse.js';
import { uploadFile } from './fileUpload.js';
import { createRecorder } from './recorder.js';
import { handleJsonResponse } from './resultHandler.js';

function setActiveTabUI(tab) {
  const tabRandom = document.getElementById('tabRandom');
  const tabFile = document.getElementById('tabFile');
  const tabRecord = document.getElementById('tabRecord');
  tabRandom.classList.remove('tab-active');
  tabFile.classList.remove('tab-active');
  tabRecord.classList.remove('tab-active');
  if (tab === 'random') tabRandom.classList.add('tab-active');
  if (tab === 'file') tabFile.classList.add('tab-active');
  if (tab === 'record') tabRecord.classList.add('tab-active');
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

  function showRandom() { panelRandom.style.display = ''; panelBrowse.style.display = 'none'; panelFile.style.display = 'none'; panelRecord.style.display = 'none'; setActiveTabUI('random'); }
  function showFile() { panelRandom.style.display = 'none'; panelBrowse.style.display = 'none'; panelFile.style.display = ''; panelRecord.style.display = 'none'; setActiveTabUI('file'); }
  function showBrowse() { panelRandom.style.display = 'none'; panelBrowse.style.display = ''; panelFile.style.display = 'none'; panelRecord.style.display = 'none'; setActiveTabUI('browse'); }
  function showRecord() { panelRandom.style.display = 'none'; panelFile.style.display = 'none'; panelRecord.style.display = ''; setActiveTabUI('record'); }

  tabRandom.addEventListener('click', showRandom);
  const tabBrowse = document.getElementById('tabBrowse');
  tabBrowse.addEventListener('click', showBrowse);
  tabFile.addEventListener('click', showFile);
  tabRecord.addEventListener('click', showRecord);

  regenBtn.addEventListener('click', () => generateAndSend(regenBtn, handleJsonResponse, setLoading));

  const browseInput = document.getElementById('browseInput');
  // attach filtering to prevent invalid characters
  attachBrowseInputFilter(browseInput);
  const doBrowseGenerateBtn = document.getElementById('doBrowseGenerate');
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

  doFileSearch.addEventListener('click', async () => {
    const file = fileInput.files && fileInput.files[0];
    if (!file) return alert('Please select a .wav file first');
    await uploadFile(file, handleJsonResponse, setLoading);
  });

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

  // default
  showRandom();
});
