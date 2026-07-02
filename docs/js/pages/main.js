// Entry point for the Search page: wires up reconstruct-from-index,
// generate-random, upload-WAV, and metadata search actions.

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
  searchByCover,
} from '../utils/metadataSearch.js';
import { quantizeImageToCoverPixels } from '../utils/coverImage.js';
import { setFindInLibraryTarget } from '../utils/findInLibrary.js';
import { escapeHtml } from '../utils/dom.js';
import { handleError } from '../utils/errorHandler.js';

function getWavOptions() {
  const bitDepthSelect = document.getElementById('bitDepthSelect');
  const sampleRateSelect = document.getElementById('sampleRateSelect');
  return {
    bitDepth: bitDepthSelect ? Number(bitDepthSelect.value) : undefined,
    sampleRate: sampleRateSelect ? Number(sampleRateSelect.value) : undefined,
  };
}

// Adds the uploaded WAV's actual property as an option first if the dropdown
// doesn't already offer it.
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

// Wires up the "By Index" / "By Metadata" tab buttons to show/hide their panels.
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

// Wires up the "By Metadata" tab: search by any combination of
// genre/artist/album/track names, or by a target cover-art image, and jump a
// result into Browse.
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
  const moreBtn = document.getElementById('metadataMoreBtn');
  const coverInput = document.getElementById('metaCoverInput');
  const coverClearBtn = document.getElementById('metaCoverClearBtn');
  const coverPreview = document.getElementById('metaCoverPreview');
  const coverCanvas = document.getElementById('metaCoverCanvas');
  if (!searchBtn || !statusEl || !resultsList || Object.values(fieldInputs).some((el) => !el))
    return;

  let nameWidths = null;
  // Cover mosaic side length in tiles; the real value arrives with the
  // library constants, this is just a safe default until then.
  let coverPixelsPerSide = 64;
  // Quantized RGB pixels of the currently selected cover image, or null when
  // searching by names instead.
  let coverPixels = null;

  getWasmModule()
    .then((wasm) => {
      const constants = JSON.parse(wasm.module.getLibraryConstants());
      nameWidths = computeNameWidths(constants);
      if (constants.coverPixelsPerSide > 0) coverPixelsPerSide = constants.coverPixelsPerSide;
      Object.entries(fieldInputs).forEach(([level, inputEl]) => {
        const width = nameWidths[level];
        inputEl.maxLength = width;
      });
    })
    .catch((error) =>
      handleError('main.js:initMetadataSearch', error, 'Failed to load library constants')
    );

  const updateSearchButtonState = () => {
    const anyFilled = Object.values(fieldInputs).some((el) => el.value.trim().length > 0);
    searchBtn.toggleAttribute('disabled', !coverPixels && !anyFilled);
  };

  // A chosen cover image takes over the search: pin the cover, ignore names.
  // The name fields are disabled while an image is set so that's visible.
  const setCoverPixels = (pixels) => {
    coverPixels = pixels;
    Object.values(fieldInputs).forEach((el) => el.toggleAttribute('disabled', !!pixels));
    updateSearchButtonState();
  };

  if (coverInput && coverClearBtn && coverPreview && coverCanvas) {
    coverInput.addEventListener('change', async () => {
      const file = coverInput.files?.[0];
      if (!file) return;
      try {
        const { pixels, imageData } = await quantizeImageToCoverPixels(file, coverPixelsPerSide);
        coverCanvas.width = coverPixelsPerSide;
        coverCanvas.height = coverPixelsPerSide;
        coverCanvas.getContext('2d').putImageData(imageData, 0, 0);
        coverPreview.removeAttribute('hidden');
        coverClearBtn.removeAttribute('hidden');
        setCoverPixels(pixels);
      } catch (error) {
        coverInput.value = '';
        handleError('main.js:metaCoverInput', error, 'Could not read that image file.');
      }
    });

    coverClearBtn.addEventListener('click', () => {
      coverInput.value = '';
      coverPreview.setAttribute('hidden', '');
      coverClearBtn.setAttribute('hidden', '');
      setCoverPixels(null);
    });
  }

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

  async function runMetadataSearch(append = false) {
    if (searchBtn.hasAttribute('disabled')) return;

    const fields = {
      genre: fieldInputs.genre.value.trim(),
      artist: fieldInputs.artist.value.trim(),
      album: fieldInputs.album.value.trim(),
      track: fieldInputs.track.value.trim(),
    };

    searchBtn.setAttribute('disabled', '');
    if (!append) {
      statusEl.textContent = 'Searching…';
      resultsList.innerHTML = '';
      if (moreBtn) moreBtn.style.display = 'none';
    }

    try {
      const wasm = await getWasmModule();
      const matches = coverPixels
        ? await searchByCover(wasm, coverPixels, { maxResults: 10 })
        : await searchByMetadata(wasm, fields, { maxResults: 10 });

      if (matches.length === 0 && !append) {
        statusEl.textContent = 'No matches found.';
      } else {
        const total = resultsList.children.length + matches.length;
        statusEl.textContent = `Found ${total} candidate${total === 1 ? '' : 's'}.`;
        matches.forEach((match) => resultsList.appendChild(renderMetadataResult(match)));
        if (moreBtn) moreBtn.style.display = matches.length === 0 ? 'none' : 'block';
      }
    } catch (error) {
      statusEl.textContent = '';
      handleError('main.js:runMetadataSearch', error, error.message);
    } finally {
      updateSearchButtonState();
    }
  }

  searchBtn.addEventListener('click', () => runMetadataSearch(false));
  if (moreBtn) moreBtn.addEventListener('click', () => runMetadataSearch(true));
}
