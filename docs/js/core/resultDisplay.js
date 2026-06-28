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
    resultFrag = await loadFragment('#resultContainer', './components/result.html?v=3');
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
 * Displays a JSON response containing audio metadata and WAV data.
 * @param {Object} j - JSON response object with metadata and wavBase64 properties
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

  // Calculate and display position in library using C++ WASM
  const positionDisplay = frag.get('#positionDisplay');
  if (positionDisplay && indexToShow) {
    try {
      const wasm = await getWasmModule();
      const positionJson = wasm.module.calculatePosition(indexToShow);
      const position = JSON.parse(positionJson);

      if (position.error) {
        throw new Error(position.error);
      }

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

  // audio
  if (j.wavBase64) {
    const bytes = atob(j.wavBase64);
    const ab = new Uint8Array(bytes.length);
    for (let i = 0; i < bytes.length; ++i) ab[i] = bytes.charCodeAt(i);
    const blob = new Blob([ab], { type: 'audio/wav' });
    const url = URL.createObjectURL(blob);
    const downloadLink = frag.get('#downloadLink');
    if (downloadLink) downloadLink.href = url;

    // Generate waveform visualization with WaveSurfer.js
    const waveformContainer = frag.get('#waveformContainer');
    if (waveformContainer) {
      try {
        // Destroy previous instance if it exists
        if (wavesurferInstance) {
          wavesurferInstance.destroy();
          wavesurferInstance = null;
        }

        // Create new WaveSurfer instance with playback controls
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

        // Show total duration once waveform is ready (register before loadBlob to avoid race)
        wavesurferInstance.on('ready', () => {
          const dur = wavesurferInstance.getDuration();
          const durationEl = frag.get('#waveformDuration');
          if (durationEl && dur) {
            const formatted =
              dur < 60
                ? `${Math.round(dur * 10) / 10}s`
                : `${Math.round(dur / 60)}m${Math.round(dur % 60)}s`;
            durationEl.textContent = `${formatted}`;
            durationEl.style.display = 'block';
          }
        });

        // Load the audio blob
        await wavesurferInstance.loadBlob(blob);

        // Set up play/pause button
        const playPauseBtn = frag.get('#playPauseBtn');
        if (playPauseBtn) {
          // Update button text based on playback state
          const updateButton = () => {
            if (wavesurferInstance.isPlaying()) {
              playPauseBtn.textContent = '⏸ Pause';
            } else {
              playPauseBtn.textContent = '▶ Play';
            }
          };

          // Button click handler
          playPauseBtn.onclick = () => {
            wavesurferInstance.playPause();
          };

          // Listen to play/pause events to update button
          wavesurferInstance.on('play', updateButton);
          wavesurferInstance.on('pause', updateButton);
          wavesurferInstance.on('finish', updateButton);

          // Initialize button state
          updateButton();
        }

        // Add click to play/pause functionality on waveform
        wavesurferInstance.on('interaction', () => {
          wavesurferInstance.playPause();
        });
      } catch (error) {
        console.error('Error generating waveform with WaveSurfer.js:', error);
      }
    }
  }

  if (resultEl) resultEl.style.display = 'block';
}
