/**
 * main.js
 *
 * Main entry point for the consolidated Search page: wires up the
 * reconstruct-from-index, generate-random, and upload-WAV actions.
 */

import {
  generateFromIndex,
  generateRandom,
  extractIndexFromWav,
  attachSearchInputFilter,
} from './search.js';
import { handleJsonResponse } from '../core/resultDisplay.js';
import { getWasmModule } from '../core/wasmModule.js';
import {
  computeNameWidths,
  sanitizeMetadataFieldValue,
  searchByMetadata,
} from '../utils/metadataSearch.js';
import { setFindInLibraryTarget } from '../utils/findInLibrary.js';
import { escapeHtml } from '../utils/dom.js';
import { handleError } from '../utils/errorHandler.js';

/**
 * Read the current Advanced Options dropdown selections.
 * @returns {{sampleRate: number, bitDepth: number}}
 */
function getWavOptions() {
  const bitDepthSelect = document.getElementById('bitDepthSelect');
  const sampleRateSelect = document.getElementById('sampleRateSelect');
  return {
    bitDepth: bitDepthSelect ? Number(bitDepthSelect.value) : undefined,
    sampleRate: sampleRateSelect ? Number(sampleRateSelect.value) : undefined,
  };
}

/**
 * Select a value in a <select>, adding it as an option first if the
 * uploaded WAV's actual property isn't already one of the dropdown's choices.
 * @param {HTMLSelectElement} selectEl - Select element to update
 * @param {number} value - Value to select
 */
function setSelectValue(selectEl, value) {
  if (!selectEl || value === undefined || value === null) return;

  const stringValue = String(value);
  const hasOption = Array.from(selectEl.options).some((opt) => opt.value === stringValue);
  if (!hasOption) {
    const option = document.createElement('option');
    option.value = stringValue;
    option.textContent = stringValue;
    selectEl.appendChild(option);
  }
  selectEl.value = stringValue;
}

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
    controls.forEach((c) => c && c.setAttribute('disabled', ''));
    if (resultContainer) resultContainer.classList.add('skeleton');
    if (loadingOverlay) loadingOverlay.classList.add('active');
  } else {
    if (spinner) spinner.style.display = 'none';
    if (msg) msg.textContent = 'Ready';
    controls.forEach((c) => c && c.removeAttribute('disabled'));
    if (resultContainer) resultContainer.classList.remove('skeleton');
    if (loadingOverlay) loadingOverlay.classList.remove('active');
    // Re-evaluate the search button's own disabled-when-empty rule
    const searchInput = document.getElementById('searchInput');
    if (searchInput) searchInput.dispatchEvent(new Event('input'));
  }
}

document.addEventListener('DOMContentLoaded', () => {
  // Search by index: input and generate button
  const searchInput = document.getElementById('searchInput');
  const doSearchGenerate = document.getElementById('doSearchGenerate');
  if (searchInput && doSearchGenerate) {
    attachSearchInputFilter(searchInput);

    doSearchGenerate.addEventListener('click', () => {
      generateFromIndex(searchInput, handleJsonResponse, setLoading, getWavOptions());
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
    doRandomGenerate.addEventListener('click', () =>
      generateRandom(handleJsonResponse, setLoading, searchInput, getWavOptions())
    );
  }

  // Upload WAV file: derive its index and drop it straight into the index input
  const fileInput = document.getElementById('fileInput');
  const clearFileBtn = document.getElementById('clearFileBtn');
  const bitDepthSelect = document.getElementById('bitDepthSelect');
  const sampleRateSelect = document.getElementById('sampleRateSelect');
  if (fileInput && searchInput) {
    fileInput.addEventListener('change', async () => {
      const file = fileInput.files?.[0];
      clearFileBtn?.toggleAttribute('hidden', !file);
      if (file) {
        const wavProps = await extractIndexFromWav(file, searchInput, setLoading);
        if (wavProps) {
          setSelectValue(bitDepthSelect, wavProps.bitDepth);
          setSelectValue(sampleRateSelect, wavProps.sampleRate);
        }
      }
    });

    clearFileBtn?.addEventListener('click', () => {
      fileInput.value = '';
      clearFileBtn.setAttribute('hidden', '');
    });
  }

  initSearchTabs();
  initMetadataSearch();
});

/**
 * Wire up the "By Index" / "By Metadata" tab buttons to show/hide their panels.
 */
function initSearchTabs() {
  const tabs = [
    { btn: document.getElementById('tabByIndex'), panel: document.getElementById('byIndexPanel') },
    {
      btn: document.getElementById('tabByMetadata'),
      panel: document.getElementById('byMetadataPanel'),
    },
  ];
  if (!tabs.every(({ btn, panel }) => btn && panel)) return;

  tabs.forEach(({ btn }, i) => {
    btn.addEventListener('click', () => {
      tabs.forEach(({ btn: otherBtn, panel: otherPanel }, j) => {
        const active = i === j;
        otherPanel.hidden = !active;
        otherBtn.classList.toggle('tab-active', active);
        otherBtn.classList.toggle('btn-secondary', !active);
        otherBtn.setAttribute('aria-selected', String(active));
      });
    });
  });
}

/**
 * Wire up the "By Metadata" tab: searching by any combination of
 * genre/artist/album/track names, and jumping a result into Browse.
 */
function initMetadataSearch() {
  const fieldInputs = {
    genre: document.getElementById('metaGenreInput'),
    artist: document.getElementById('metaArtistInput'),
    album: document.getElementById('metaAlbumInput'),
    track: document.getElementById('metaTrackInput'),
  };
  const searchBtn = document.getElementById('doMetadataSearch');
  const statusEl = document.getElementById('metadataSearchStatus');
  const resultsList = document.getElementById('metadataResultsList');
  if (!searchBtn || !statusEl || !resultsList || Object.values(fieldInputs).some((el) => !el))
    return;

  let nameWidths = null;
  getWasmModule()
    .then((wasm) => {
      const constants = JSON.parse(wasm.module.getLibraryConstants());
      nameWidths = computeNameWidths(constants);
      Object.entries(fieldInputs).forEach(([level, inputEl]) => {
        const width = nameWidths[level];
        inputEl.maxLength = width;
        inputEl.placeholder = `${level[0].toUpperCase()}${level.slice(1)} name (up to ${width} chars)`;
      });
    })
    .catch((error) =>
      handleError('main.js:initMetadataSearch', error, 'Failed to load library constants')
    );

  const updateSearchButtonState = () => {
    const anyFilled = Object.values(fieldInputs).some((el) => el.value.trim().length > 0);
    searchBtn.toggleAttribute('disabled', !anyFilled);
  };

  Object.entries(fieldInputs).forEach(([level, inputEl]) => {
    inputEl.addEventListener('input', () => {
      const width = nameWidths ? nameWidths[level] : inputEl.maxLength;
      const filtered = sanitizeMetadataFieldValue(inputEl.value, width > 0 ? width : 64);
      if (filtered !== inputEl.value) inputEl.value = filtered;
      updateSearchButtonState();
    });
    inputEl.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') runMetadataSearch();
    });
  });
  updateSearchButtonState();

  function renderMetadataResult(result) {
    const { names, position } = result;
    const room = position.room === '' ? '0' : position.room;
    const shortRoom = room.length > 14 ? `${room.slice(0, 14)}…` : room;

    const li = document.createElement('li');
    li.className = 'metadata-result-item';
    li.innerHTML = `
      <div class="metadata-result-summary">
        <span class="metadata-result-names">${escapeHtml(names.genre)} / ${escapeHtml(names.artist)} / ${escapeHtml(names.album)} / ${escapeHtml(names.track)}</span>
        <span class="metadata-result-meta">Room ${escapeHtml(shortRoom)}</span>
      </div>
      <button class="btn" type="button">Find in Library &rarr;</button>
    `;
    li.querySelector('button').addEventListener('click', () => {
      setFindInLibraryTarget(position);
      window.location.href = './browse.html';
    });
    return li;
  }

  async function runMetadataSearch() {
    if (searchBtn.hasAttribute('disabled')) return;

    const fields = {
      genre: fieldInputs.genre.value.trim(),
      artist: fieldInputs.artist.value.trim(),
      album: fieldInputs.album.value.trim(),
      track: fieldInputs.track.value.trim(),
    };

    searchBtn.setAttribute('disabled', '');
    statusEl.textContent = 'Searching…';
    resultsList.innerHTML = '';

    try {
      const wasm = await getWasmModule();
      const matches = await searchByMetadata(wasm, fields, { maxResults: 10 });

      if (matches.length === 0) {
        statusEl.textContent = 'No matches found.';
      } else {
        statusEl.textContent = `Found ${matches.length} candidate${matches.length === 1 ? '' : 's'}.`;
        matches.forEach((match) => resultsList.appendChild(renderMetadataResult(match)));
      }
    } catch (error) {
      statusEl.textContent = '';
      handleError('main.js:runMetadataSearch', error, error.message);
    } finally {
      updateSearchButtonState();
    }
  }

  searchBtn.addEventListener('click', runMetadataSearch);
}
