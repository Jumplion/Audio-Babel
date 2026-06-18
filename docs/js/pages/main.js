/**
 * main.js
 *
 * Main entry point for the consolidated Search page: wires up the
 * reconstruct-from-index, generate-random, and upload-WAV actions.
 */

import { generateFromIndex, generateRandom, uploadWav, attachSearchInputFilter } from './search.js';
import { handleJsonResponse } from '../core/resultDisplay.js';
import { showValidationError } from '../utils/errorHandler.js';

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
  const doFileSearch = document.getElementById('doFileSearch');
  const controls = [doSearchGenerate, doRandomGenerate, doFileSearch];
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
    doRandomGenerate.addEventListener('click', () => generateRandom(handleJsonResponse, setLoading));
  }

  // Upload WAV file
  const fileInput = document.getElementById('fileInput');
  const doFileSearch = document.getElementById('doFileSearch');
  if (fileInput && doFileSearch) {
    doFileSearch.addEventListener('click', async () => {
      const file = fileInput.files?.[0];
      if (!file) {
        showValidationError('Please select a .wav file first');
        return;
      }
      await uploadWav(file, handleJsonResponse, setLoading);
    });
  }
});
