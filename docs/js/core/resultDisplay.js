/**
 * resultDisplay.js
 *
 * Manages the display of audio generation results.
 * Handles metadata display, expandable text, clickable indexes,
 * audio playback controls, and download functionality.
 */

import { loadFragment } from '../ui/loadFragment.js';
import { getWasmModule } from './wasmModule.js';
import { escapeHtml, downloadBlob } from '../utils/dom.js';
import { openIndexInNewTab } from './indexViewer.js';
import { setFindInLibraryTarget } from '../utils/findInLibrary.js';
import { createWavFile } from '../utils/wavUtils.js';
import { buildResultForIndex } from '../utils/resultBuilder.js';
import WaveSurfer from 'https://unpkg.com/wavesurfer.js@7/dist/wavesurfer.esm.js';
import Timeline from 'https://unpkg.com/wavesurfer.js@7/dist/plugins/timeline.esm.js';

let resultFrag = null;
let wavesurferInstance = null;

/**
 * Clean up the result handler state (WaveSurfer instance and fragment cache).
 * Call this when navigating away from a result or before generating a new result.
 */
export function cleanupResultHandler() {
  if (wavesurferInstance) {
    wavesurferInstance.destroy();
    wavesurferInstance = null;
  }
  resultFrag = null;
}

/**
 * Truncate a string if it exceeds a threshold, showing the first and last
 * `partLength` characters joined by an ellipsis.
 * @param {string} str - String to truncate
 * @param {number} threshold - Length above which truncation kicks in (default: 30)
 * @param {number} [partLength] - Characters to keep at each end (default: 40% of threshold)
 * @returns {string} Truncated string with ellipsis if needed
 */
function truncateString(str, threshold = 30, partLength = Math.floor(threshold * 0.4)) {
  if (!str || str.length <= threshold) return str;

  const start = str.substring(0, partLength);
  const end = str.substring(str.length - partLength);
  return `${start}...${end}`;
}

/**
 * Pick a single cosmetic name out of a getXNames() JSON array response,
 * falling back to the raw numeric index if the names couldn't be loaded.
 * @param {string} namesJson - JSON array string returned by a getXNames() wasm call
 * @param {number} index - Slot to pick (wall/shelf/album/track number)
 * @returns {string} The name at that slot, or the index as a string
 */
function resolveCosmeticName(namesJson, index) {
  try {
    const names = JSON.parse(namesJson);
    if (Array.isArray(names) && names[index]) return names[index];
  } catch (e) {
    // Fall through to the index fallback below
  }
  return String(index);
}

/**
 * Ensures the result fragment component is loaded.
 * Loads result.html fragment into #resultContainer on first call, then caches it.
 * @returns {Promise<Object>} Fragment helper object with get/getAll methods
 */
export async function ensureResultFrag() {
  if (!resultFrag)
    resultFrag = await loadFragment('#resultContainer', './components/result.html?v=4');
  return resultFrag;
}

/**
 * Toggle the expanded state of metadata
 * @param {HTMLElement} element - Metadata element
 * @param {string} expandedId - Unique ID for expanded section
 * @param {string} fullText - Complete text to show when expanded
 * @param {string} truncatedText - Truncated text to show when collapsed
 */
function toggleMetadataExpansion(element, expandedId, fullText, truncatedText) {
  const existingExpanded = document.getElementById(expandedId);

  if (existingExpanded) {
    // Collapse: remove expanded view
    existingExpanded.remove();
    element.textContent = truncatedText;
  } else {
    // Expand: create and insert expanded view
    const expandedDiv = document.createElement('div');
    expandedDiv.id = expandedId;
    expandedDiv.className = 'metadata-expanded';
    expandedDiv.textContent = fullText;

    element.parentNode.insertBefore(expandedDiv, element.nextSibling);
    element.textContent = truncatedText + ' ▼';
  }
}

/**
 * Create a clickable metadata element that can expand to show full text
 * @param {HTMLElement} element - The metadata element
 * @param {string} fullText - The complete metadata string
 * @param {string} truncatedText - The truncated version to display initially
 * @param {string} fieldName - Name of the field (e.g., 'genre', 'artist')
 */
function makeMetadataExpandable(element, fullText, truncatedText, fieldName) {
  if (!element || !fullText) return;

  const expandedId = `expanded-${fieldName}`;

  // Set initial state
  element.textContent = truncatedText;
  element.classList.add('metadata-expandable');
  element.title = 'Click to expand/collapse';

  // Add toggle handler
  element.addEventListener('click', () => {
    toggleMetadataExpansion(element, expandedId, fullText, truncatedText);
  });
}

/**
 * Create a clickable index display that downloads and opens the full index in a new tab
 * @param {HTMLElement} indexDisplay - The index display element
 * @param {string} fullIndex - The complete index string
 * @returns {HTMLElement} Updated index display element
 */
function makeIndexClickable(indexDisplay, fullIndex) {
  if (!indexDisplay || !fullIndex) return indexDisplay;

  // Remove existing handlers by cloning
  const newIndexDisplay = indexDisplay.cloneNode(false);
  indexDisplay.parentNode.replaceChild(newIndexDisplay, indexDisplay);

  // Set truncated display text
  newIndexDisplay.textContent = truncateString(fullIndex, 200, 100);
  newIndexDisplay.classList.add('index-clickable');
  newIndexDisplay.title = 'Click to download and view full index';

  // Add click handler for download + view
  newIndexDisplay.addEventListener('click', () => {
    const blob = new Blob([fullIndex], { type: 'text/plain' });
    downloadBlob(blob, 'audio-index.txt');
    openIndexInNewTab(fullIndex);
  });

  return newIndexDisplay;
}

/**
 * Build a WAV Blob directly from raw PCM bytes, skipping any base64 conversion.
 * Used for both waveform loading and lazy download — the same Blob serves both.
 * @param {Uint8Array} pcm
 * @param {number} sampleRate
 * @param {number} bitDepth
 * @param {number} numChannels
 * @returns {Blob}
 */
const pcmToWavBlob = (pcm, sampleRate, bitDepth, numChannels) =>
  createWavFile(pcm, sampleRate, bitDepth, numChannels);

/**
 * Displays a JSON response containing audio metadata and PCM data.
 * @param {Object} j - Result object from buildResultForIndex
 * @param {string} [originalIndexB64] - Optional original index string to display
 */
export async function handleJsonResponse(j, originalIndexB64) {
  const frag = await ensureResultFrag();
  let indexDisplay = frag.get('#indexDisplay');
  const resultEl = frag.get('#result');

  // show index with truncation and click-to-download functionality
  const indexToShow = originalIndexB64 || j.indexBase64 || '';
  if (indexDisplay && indexToShow) {
    indexDisplay = makeIndexClickable(indexDisplay, indexToShow);
  }

  // Display position in library — position is pre-computed by buildResultForIndex.
  const positionDisplay = frag.get('#positionDisplay');
  if (positionDisplay && j.position) {
    try {
      const wasm = await getWasmModule();
      const position = j.position;

      // Truncate room display if it's too long
      const room = position.room === '' ? '0' : position.room;
      const roomDisplay = truncateString(room, 20, 8);

      // Resolve the cosmetic Genre/Artist/Album/Track names for this position,
      // using the same per-level naming calls the Browse page uses.
      const genreName = resolveCosmeticName(wasm.module.getGenreNames(room), position.wall);
      const artistName = resolveCosmeticName(
        wasm.module.getArtistNames(room, position.wall),
        position.shelf
      );
      const albumName = resolveCosmeticName(
        wasm.module.getAlbumNames(room, position.wall, position.shelf),
        position.album
      );
      const trackName = resolveCosmeticName(
        wasm.module.getTrackNames(room, position.wall, position.shelf, position.album),
        position.track
      );

      positionDisplay.innerHTML = `
        <div class="position-grid">
          <div class="position-row">
            <span class="position-field"><strong>Room:</strong> <code class="position-value">${escapeHtml(roomDisplay)}</code></span>
          </div>
          <div class="position-row">
            <span class="position-field"><strong>Genre:</strong> <span class="position-value">${escapeHtml(genreName)}</span></span>
            <span class="position-field"><strong>Artist:</strong> <span class="position-value">${escapeHtml(artistName)}</span></span>
          </div>
          <div class="position-row">
            <span class="position-field"><strong>Album:</strong> <span class="position-value">${escapeHtml(albumName)}</span></span>
            <span class="position-field"><strong>Track:</strong> <span class="position-value">${escapeHtml(trackName)}</span></span>
          </div>
        </div>
      `;

      // Only offer to jump into Browse from pages other than Browse itself —
      // on Browse, the user is already standing at this exact position.
      const findInLibraryBtn = frag.get('#findInLibraryBtn');
      if (findInLibraryBtn) {
        if (document.body.classList.contains('browse')) {
          findInLibraryBtn.style.display = 'none';
        } else {
          findInLibraryBtn.style.display = '';
          findInLibraryBtn.onclick = () => {
            setFindInLibraryTarget({
              room: position.room,
              wall: position.wall,
              shelf: position.shelf,
              album: position.album,
              track: position.track,
            });
            window.location.href = './browse.html';
          };
        }
      }
    } catch (error) {
      console.error('Error calculating position:', error);
      positionDisplay.textContent = 'Unable to calculate position';

      const findInLibraryBtn = frag.get('#findInLibraryBtn');
      if (findInLibraryBtn) findInLibraryBtn.style.display = 'none';
    }
  }

  // metadata with expandable sections
  if (j.metadata) {
    const g = frag.get('#metaGenre');
    const a = frag.get('#metaArtist');
    const al = frag.get('#metaAlbum');
    const t = frag.get('#metaTrack');

    // Make each metadata field expandable
    if (g) {
      const genreText = j.metadata.genre || '';
      makeMetadataExpandable(g, genreText, truncateString(genreText, 30), 'genre');
    }
    if (a) {
      const artistText = j.metadata.artist || '';
      makeMetadataExpandable(a, artistText, truncateString(artistText, 30), 'artist');
    }
    if (al) {
      const albumText = j.metadata.album || '';
      makeMetadataExpandable(al, albumText, truncateString(albumText, 30), 'album');
    }
    if (t) {
      const trackText = j.metadata.track || '';
      makeMetadataExpandable(t, trackText, truncateString(trackText, 30), 'track');
    }

    const cover = frag.get('#coverImg');
    const metadataEl = frag.get('#metadata');
    if (cover && j.metadata.cover) {
      // Convert SVG string to data URL for img src
      const svgDataUrl = 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(j.metadata.cover);
      cover.src = svgDataUrl;
      if (metadataEl) metadataEl.style.display = '';
    } else if (cover && metadataEl) {
      cover.src = '';
      metadataEl.style.display = 'none';
    }
  }

  var DEFAULT_SAMPLE_RATE = 44100;
  var DEFAULT_BIT_DEPTH = 16;
  var DEFAULT_NUM_CHANNELS = 1;

  // audio
  if (j.pcm) {
    const {
      sampleRate = DEFAULT_SAMPLE_RATE,
      bitDepth = DEFAULT_BIT_DEPTH,
      numChannels = DEFAULT_NUM_CHANNELS,
    } = j;

    // Build the WAV Blob once from raw PCM — no base64 step.
    // The same Blob feeds both WaveSurfer and the download link.
    const wavBlob = pcmToWavBlob(j.pcm, sampleRate, bitDepth, numChannels);

    const downloadLink = frag.get('#downloadLink');
    if (downloadLink) {
      downloadLink.href = '#';
      downloadLink.onclick = (e) => {
        e.preventDefault();
        downloadBlob(wavBlob, 'reconstructed.wav');
      };
    }

    const waveformContainer = frag.get('#waveformContainer');
    if (waveformContainer) {
      try {
        if (wavesurferInstance) {
          wavesurferInstance.destroy();
          wavesurferInstance = null;
        }

        wavesurferInstance = WaveSurfer.create({
          container: waveformContainer,
          waveColor: '#cf8a48',
          progressColor: '#d9ac5e',
          cursorColor: '#f1e7d6',
          barWidth: 2,
          barRadius: 3,
          cursorWidth: 2,
          height: 160,
          barGap: 1,
          normalize: true,
          interact: true,
          dragToSeek: true,
          plugins: [
            Timeline.create({
              height: 22,
              style: { color: 'rgba(207, 138, 72, 0.55)', fontSize: '10px' },
            }),
          ],
        });

        wavesurferInstance.on('ready', () => {
          const dur = wavesurferInstance.getDuration();
          const durationEl = frag.get('#waveformDuration');
          if (durationEl && dur) {
            const formatted =
              dur < 60
                ? `${Math.round(dur * 10) / 10}s`
                : `${Math.round(dur / 60)}m${Math.round(dur % 60)}s`;
            durationEl.textContent = formatted;
            durationEl.style.display = 'block';
          }
        });

        await wavesurferInstance.loadBlob(wavBlob);

        const playPauseBtn = frag.get('#playPauseBtn');
        if (playPauseBtn) {
          const updateButton = () => {
            playPauseBtn.textContent = wavesurferInstance.isPlaying() ? '⏸ Pause' : '▶ Play';
          };
          playPauseBtn.onclick = () => wavesurferInstance.playPause();
          wavesurferInstance.on('play', updateButton);
          wavesurferInstance.on('pause', updateButton);
          wavesurferInstance.on('finish', updateButton);
          updateButton();
        }

        wavesurferInstance.on('interaction', () => wavesurferInstance.playPause());
      } catch (error) {
        console.error('Error generating waveform with WaveSurfer.js:', error);
      }
    }
  }

  if (resultEl) resultEl.style.display = 'block';
}
