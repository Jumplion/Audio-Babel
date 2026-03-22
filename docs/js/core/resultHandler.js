/**
 * resultHandler.js
 * 
 * Manages the display of audio generation results.
 * Handles metadata display, expandable text, clickable indexes,
 * audio playback controls, and download functionality.
 */

import { loadFragment } from '../ui/loadFragment.js';
import { getWasmModule } from './wasmModule.js';
import { escapeHtml } from '../utils/utils.js';
import WaveSurfer from 'https://unpkg.com/wavesurfer.js@7/dist/wavesurfer.esm.js';

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
 * Truncate a string if it exceeds maxLength, showing first and last parts with ellipsis
 * @param {string} str - String to truncate
 * @param {number} maxLength - Maximum length before truncation (default: 30)
 * @returns {string} Truncated string with ellipsis if needed
 */
function truncateString(str, maxLength = 30) {
  if (!str || str.length <= maxLength) return str;
  
  // Show first 40% and last 40% of the string with "..." in middle
  const partLength = Math.floor(maxLength * 0.4);
  const start = str.substring(0, partLength);
  const end = str.substring(str.length - partLength);
  return `${start}...${end}`;
}

/**
 * Ensures the result fragment component is loaded.
 * Loads result.html fragment into #resultContainer on first call, then caches it.
 * @returns {Promise<Object>} Fragment helper object with get/getAll methods
 */
export async function ensureResultFrag() {
  if (!resultFrag) resultFrag = await loadFragment('#resultContainer', './components/result.html');
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
 * Trigger a file download from a blob
 * @param {Blob} blob - File content as blob
 * @param {string} filename - Name for downloaded file
 */
function downloadBlob(blob, filename) {
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = filename;
  
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  
  // Clean up blob URL
  setTimeout(() => URL.revokeObjectURL(url), 100);
}

/**
 * Generate HTML for index viewer page
 * @param {string} indexContent - Full index string to display
 * @returns {string} Complete HTML page
 */
function generateIndexViewerHTML(indexContent) {
  return `
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Audio Index</title>
      <style>
        body {
          font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
          background: #1a1f2e;
          color: #e0e0e0;
          padding: 20px;
          margin: 0;
        }
        pre {
          white-space: pre-wrap;
          word-break: break-all;
          background: #0f1419;
          padding: 20px;
          border-radius: 8px;
          border: 1px solid #2a3f5f;
          line-height: 1.5;
        }
        h1 {
          color: #64b5f6;
          margin-bottom: 20px;
        }
      </style>
    </head>
    <body>
      <h1>Audio Index</h1>
      <pre>${escapeHtml(indexContent)}</pre>
    </body>
    </html>
  `;
}

/**
 * Open index content in a new browser tab
 * @param {string} fullIndex - Complete index string
 */
function openIndexInNewTab(fullIndex) {
  const newWindow = window.open('', '_blank');
  if (newWindow) {
    newWindow.document.write(generateIndexViewerHTML(fullIndex));
    newWindow.document.close();
  }
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
  const maxDisplayLength = 200;
  const truncated = fullIndex.length > maxDisplayLength
    ? fullIndex.substring(0, 100) + '...' + fullIndex.substring(fullIndex.length - 100)
    : fullIndex;
  
  newIndexDisplay.textContent = truncated;
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
      let roomDisplay = position.room === "" ? "0" : position.room;
      const maxRoomLength = 20;
      if (roomDisplay.length > maxRoomLength) {
        const start = roomDisplay.substring(0, 8);
        const end = roomDisplay.substring(roomDisplay.length - 8);
        roomDisplay = `${start}...${end}`;
      }
      
      positionDisplay.innerHTML = `
        <div style="display: flex; gap: 16px; flex-wrap: wrap; font-size: 14px;">
          <span><strong>Room:</strong> <code style="font-size: 13px;">${escapeHtml(roomDisplay)}</code></span>
          <span><strong>Wall:</strong> ${escapeHtml(position.wall)}</span>
          <span><strong>Shelf:</strong> ${escapeHtml(position.shelf)}</span>
          <span><strong>Album:</strong> ${escapeHtml(position.album)}</span>
          <span><strong>Track:</strong> ${escapeHtml(position.track)}</span>
        </div>
      `;
    } catch (error) {
      console.error('Error calculating position:', error);
      positionDisplay.textContent = 'Unable to calculate position';
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
          waveColor: '#64b5f6',
          progressColor: '#2196f3',
          cursorColor: '#1976d2',
          barWidth: 2,
          barRadius: 3,
          cursorWidth: 2,
          height: 120,
          barGap: 1,
          normalize: false, // Don't normalize - show actual amplitudes
          backend: 'WebAudio',
          interact: true, // Enable interaction for playback control
          dragToSeek: true, // Allow seeking by clicking/dragging
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