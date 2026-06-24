/**
 * main.js
 *
 * Main entry point for the consolidated Search page: wires up the
 * reconstruct-from-index, generate-random, and upload-WAV actions.
 */

import { generateFromIndex, generateRandom, extractIndexFromWav, attachSearchInputFilter } from './search.js';
import { handleJsonResponse } from '../core/resultDisplay.js';

/**
 * Set loading state for the UI
 * Shows/hides spinner, updates status message, and disables/enables controls
 * Also controls the loading overlay for better visual feedback
 * @param {boolean} on - True to show loading state, false to hide
 */
function setLoading(on) {
  const spinner = document.getElementById('statusSpinner');
  const msg = document.getElementById('statusMsg');
  const doSearchGenerate = document.getElementById('doSearchGenerate');
  const doRandomGenerate = document.getElementById('doRandomGenerate');
  const fileInput = document.getElementById('fileInput');
  const controls = [doSearchGenerate, doRandomGenerate, fileInput];
  const resultContainer = document.getElementById('resultContainer');
  const loadingOverlay = document.getElementById('loadingOverlay');

  if (on) {
    if (spinner) spinner.style.display = 'inline-block';
    if (msg) msg.textContent = 'Loading...';
    controls.forEach((c) => c && c.classList.add('disabled'));
    if (resultContainer) resultContainer.classList.add('skeleton');
    if (loadingOverlay) loadingOverlay.classList.add('active');
  } else {
    if (spinner) spinner.style.display = 'none';
    if (msg) msg.textContent = 'Ready';
    controls.forEach((c) => c && c.classList.remove('disabled'));
    if (resultContainer) resultContainer.classList.remove('skeleton');
    if (loadingOverlay) loadingOverlay.classList.remove('active');
  }
}

document.addEventListener('DOMContentLoaded', () => {
  // Search by index: input and generate button
  const searchInput = document.getElementById('searchInput');
  const doSearchGenerate = document.getElementById('doSearchGenerate');
  if (searchInput && doSearchGenerate) {
    attachSearchInputFilter(searchInput);

    doSearchGenerate.addEventListener('click', () => {
      generateFromIndex(searchInput, handleJsonResponse, setLoading);
    });

    const updateSearchButtonState = () => {
      const value = searchInput.value.trim();
      if (value.length > 0) {
        doSearchGenerate.removeAttribute('disabled');
      } else {
        doSearchGenerate.setAttribute('disabled', '');
      }
    };
    searchInput.addEventListener('input', updateSearchButtonState);
    updateSearchButtonState();
  }

  // Generate random index
  const doRandomGenerate = document.getElementById('doRandomGenerate');
  if (doRandomGenerate) {
    doRandomGenerate.addEventListener('click', () => generateRandom(handleJsonResponse, setLoading, searchInput));
  }

  // Upload WAV file: derive its index and drop it straight into the index input
  const clearFileBtn = document.getElementById('clearFileBtn');
  if (fileInput && searchInput) {
    fileInput.addEventListener('change', async () => {
      const file = fileInput.files?.[0];
      clearFileBtn?.toggleAttribute('hidden', !file);
      if (file) {
        await extractIndexFromWav(file, searchInput, setLoading);
      }
    });

    clearFileBtn?.addEventListener('click', () => {
      fileInput.value = '';
      clearFileBtn.setAttribute('hidden', '');
    });
  }
});
