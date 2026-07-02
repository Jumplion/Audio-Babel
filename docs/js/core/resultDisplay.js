/**
 * Manages the display of audio generation results: metadata, expandable
 * text, clickable indexes, WaveSurfer playback, WAV download, and the
 * on-demand "More Like This…" Similar Tracks list.
 */

import { loadFragment } from '../ui/loadFragment.js';
import { getWasmModule } from './wasmModule.js';
import { escapeHtml, downloadBlob } from '../utils/dom.js';
import { openIndexInNewTab } from './indexViewer.js';
import { setFindInLibraryTarget } from '../utils/findInLibrary.js';
import { createWavFile } from '../utils/wavUtils.js';
import { buildResultForIndex } from '../utils/resultBuilder.js';
import { buildSimilarTracks } from './similarTracks.js';
import {
  DEFAULT_SAMPLE_RATE,
  DEFAULT_BIT_DEPTH,
  DEFAULT_NUM_CHANNELS,
} from '../utils/audioConstants.js';
import WaveSurfer from 'https://unpkg.com/wavesurfer.js@7/dist/wavesurfer.esm.js';
import Timeline from 'https://unpkg.com/wavesurfer.js@7/dist/plugins/timeline.esm.js';

let resultFrag = null;
let wavesurferInstance = null;

// Call when navigating away from a result or before generating a new one.
export function cleanupResultHandler() {
  if (wavesurferInstance) {
    wavesurferInstance.destroy();
    wavesurferInstance = null;
  }
  resultFrag = null;
}

// Truncates a string down to `threshold` chars, keeping `partLength` chars
// from each end joined by an ellipsis.
function truncateString(str, threshold = 30, partLength = Math.floor(threshold * 0.4)) {
  if (!str || str.length <= threshold) return str;

  const start = str.substring(0, partLength);
  const end = str.substring(str.length - partLength);
  return `${start}...${end}`;
}

// Picks a single cosmetic name out of a getXNames() JSON array response,
// falling back to the raw numeric index if the names couldn't be loaded.
function resolveCosmeticName(namesJson, index) {
  try {
    const names = JSON.parse(namesJson);
    if (Array.isArray(names) && names[index]) return names[index];
  } catch (e) {
    // Fall through to the index fallback below
  }
  return String(index);
}

// Loads result.html into #resultContainer on first call, then caches it.
export async function ensureResultFrag() {
  if (!resultFrag)
    resultFrag = await loadFragment('#resultContainer', './components/result.html?v=5');
  return resultFrag;
}

function toggleMetadataExpansion(element, expandedId, fullText, truncatedText) {
  const existingExpanded = document.getElementById(expandedId);

  if (existingExpanded) {
    existingExpanded.remove();
    element.textContent = truncatedText;
  } else {
    const expandedDiv = document.createElement('div');
    expandedDiv.id = expandedId;
    expandedDiv.className = 'metadata-expanded';
    expandedDiv.textContent = fullText;

    element.parentNode.insertBefore(expandedDiv, element.nextSibling);
    element.textContent = truncatedText + ' ▼';
  }
}

function makeMetadataExpandable(element, fullText, truncatedText, fieldName) {
  if (!element || !fullText) return;

  const expandedId = `expanded-${fieldName}`;

  element.textContent = truncatedText;
  element.classList.add('metadata-expandable');
  element.title = 'Click to expand/collapse';

  element.addEventListener('click', () => {
    toggleMetadataExpansion(element, expandedId, fullText, truncatedText);
  });
}

// Replaces indexDisplay with a fresh clone (dropping old click handlers) that
// downloads and opens the full index on click.
function makeIndexClickable(indexDisplay, fullIndex) {
  if (!indexDisplay || !fullIndex) return indexDisplay;

  const newIndexDisplay = indexDisplay.cloneNode(false);
  indexDisplay.parentNode.replaceChild(newIndexDisplay, indexDisplay);

  newIndexDisplay.textContent = truncateString(fullIndex, 200, 100);
  newIndexDisplay.classList.add('index-clickable');
  newIndexDisplay.title = 'Click to download and view full index';

  newIndexDisplay.addEventListener('click', () => {
    const blob = new Blob([fullIndex], { type: 'text/plain' });
    downloadBlob(blob, 'audio-index.txt');
    openIndexInNewTab(fullIndex);
  });

  return newIndexDisplay;
}

// Builds a WAV Blob directly from raw PCM bytes (no base64 step) — the same
// Blob feeds both WaveSurfer and the download link.
const pcmToWavBlob = (pcm, sampleRate, bitDepth, numChannels) =>
  createWavFile(pcm, sampleRate, bitDepth, numChannels);

// Builds a similar-track button's label: a truncated index plus its cosmetic
// track name, fetched the same way the main metadata panel does (never
// fabricated client-side).
function describeSimilarTrack(wasm, indexBase64) {
  const indexLabel = truncateString(indexBase64, 24, 10);
  try {
    const metadata = JSON.parse(wasm.module.getMetadata(indexBase64));
    if (metadata.error) throw new Error(metadata.error);
    return { indexLabel, trackName: metadata.track || 'Unknown' };
  } catch (error) {
    console.error('Error resolving similar track metadata:', error);
    return { indexLabel, trackName: 'Unknown' };
  }
}

// Renders the "Similar Tracks" list on demand. Clicking an entry regenerates
// the whole result display for that index.
async function renderSimilarTracks(frag, wasm, pcmData) {
  const list = frag.get('#similarTracksList');
  const status = frag.get('#similarTracksStatus');
  if (!list) return;

  list.innerHTML = '';
  if (status) status.textContent = 'Generating similar tracks…';

  try {
    const variantIndexes = buildSimilarTracks(wasm, pcmData);

    if (variantIndexes.length === 0) {
      if (status) status.textContent = 'No similar tracks could be generated.';
      return;
    }

    if (status) status.textContent = '';
    variantIndexes.forEach((variantIndex) => {
      const { indexLabel, trackName } = describeSimilarTrack(wasm, variantIndex);

      const li = document.createElement('li');
      li.className = 'similar-track-item';

      const btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'btn similar-track-btn';
      btn.title = 'Click to load this similar track';
      btn.innerHTML = `<code class="similar-track-index">${escapeHtml(indexLabel)}</code><span class="similar-track-name">${escapeHtml(trackName)}</span>`;
      btn.addEventListener('click', async () => {
        try {
          const w = await getWasmModule();
          const result = await buildResultForIndex(w, variantIndex);
          await handleJsonResponse(result, variantIndex);
        } catch (error) {
          console.error('Error loading similar track:', error);
        }
      });

      li.appendChild(btn);
      list.appendChild(li);
    });
  } catch (error) {
    console.error('Error generating similar tracks:', error);
    if (status) status.textContent = 'Unable to generate similar tracks';
  }
}

// Renders a result object (see resultBuilder.js) into the result fragment:
// index, library position, metadata, and audio playback/download.
export async function handleJsonResponse(j, originalIndexB64) {
  const frag = await ensureResultFrag();
  let indexDisplay = frag.get('#indexDisplay');
  const resultEl = frag.get('#result');

  // Reset the "Similar Tracks" section for the new result — it's
  // regenerated on demand (via the "More Like This…" button below), not
  // carried over from whatever was previously displayed.
  const similarList = frag.get('#similarTracksList');
  const similarStatus = frag.get('#similarTracksStatus');
  if (similarList) similarList.innerHTML = '';
  if (similarStatus) similarStatus.textContent = '';

  const indexToShow = originalIndexB64 || j.indexBase64 || '';
  if (indexDisplay && indexToShow) {
    indexDisplay = makeIndexClickable(indexDisplay, indexToShow);
  }

  // Position is pre-computed by buildResultForIndex.
  const positionDisplay = frag.get('#positionDisplay');
  if (positionDisplay && j.position) {
    try {
      const wasm = await getWasmModule();
      const position = j.position;

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

  if (j.metadata) {
    const g = frag.get('#metaGenre');
    const a = frag.get('#metaArtist');
    const al = frag.get('#metaAlbum');
    const t = frag.get('#metaTrack');

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
      const svgDataUrl = 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(j.metadata.cover);
      cover.src = svgDataUrl;
      if (metadataEl) metadataEl.style.display = '';
    } else if (cover && metadataEl) {
      cover.src = '';
      metadataEl.style.display = 'none';
    }
  }

  if (j.pcm) {
    const {
      sampleRate = DEFAULT_SAMPLE_RATE,
      bitDepth = DEFAULT_BIT_DEPTH,
      numChannels = DEFAULT_NUM_CHANNELS,
    } = j;

    const wavBlob = pcmToWavBlob(j.pcm, sampleRate, bitDepth, numChannels);

    const downloadLink = frag.get('#downloadLink');
    if (downloadLink) {
      downloadLink.href = '#';
      downloadLink.onclick = (e) => {
        e.preventDefault();
        downloadBlob(wavBlob, 'reconstructed.wav');
      };
    }

    // "More Like This…" generates similar-track variants on demand, rather
    // than up front on every result — most results are never expanded.
    const moreLikeThisBtn = frag.get('#moreLikeThisBtn');
    if (moreLikeThisBtn) {
      moreLikeThisBtn.style.display = '';
      moreLikeThisBtn.disabled = false;
      moreLikeThisBtn.onclick = async () => {
        moreLikeThisBtn.disabled = true;
        try {
          const wasm = await getWasmModule();
          await renderSimilarTracks(frag, wasm, j.pcm);
        } finally {
          moreLikeThisBtn.disabled = false;
        }
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
  } else {
    const moreLikeThisBtn = frag.get('#moreLikeThisBtn');
    if (moreLikeThisBtn) moreLikeThisBtn.style.display = 'none';
  }

  if (resultEl) resultEl.style.display = 'block';
}
