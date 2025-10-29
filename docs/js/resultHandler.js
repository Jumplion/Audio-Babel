import { loadFragment } from './loadFragment.js';
import { calculatePositionFromBase64 } from './positionEncoder.js';

let resultFrag = null;

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
 * Create a clickable metadata element that can expand to show full text
 * @param {HTMLElement} element - The metadata element
 * @param {string} fullText - The complete metadata string
 * @param {string} truncatedText - The truncated version to display initially
 * @param {string} fieldName - Name of the field (e.g., 'genre', 'artist')
 */
function makeMetadataExpandable(element, fullText, truncatedText, fieldName) {
  if (!element || !fullText) return;
  
  // Create a unique ID for the expanded view
  const expandedId = `expanded-${fieldName}`;
  
  // Set initial truncated text and apply expandable styling
  element.textContent = truncatedText;
  element.classList.add('metadata-expandable');
  element.title = 'Click to expand/collapse';
  
  // Add click handler
  element.addEventListener('click', () => {
    const existingExpanded = document.getElementById(expandedId);
    
    if (existingExpanded) {
      // Remove expanded view
      existingExpanded.remove();
      element.textContent = truncatedText;
    } else {
      // Create expanded view
      const expandedDiv = document.createElement('div');
      expandedDiv.id = expandedId;
      expandedDiv.className = 'metadata-expanded';
      expandedDiv.textContent = fullText;
      
      // Insert after the element
      element.parentNode.insertBefore(expandedDiv, element.nextSibling);
      element.textContent = truncatedText + ' ▼';
    }
  });
}

/**
 * Create a clickable index display that downloads and opens the full index in a new tab
 * @param {HTMLElement} indexDisplay - The index display element
 * @param {string} fullIndex - The complete index string
 */
function makeIndexClickable(indexDisplay, fullIndex) {
  if (!indexDisplay || !fullIndex) return;
  
  // Remove any existing click handlers by cloning the element
  const newIndexDisplay = indexDisplay.cloneNode(false);
  indexDisplay.parentNode.replaceChild(newIndexDisplay, indexDisplay);
  
  // Truncate the display text
  const maxDisplayLength = 200;
  const truncated = fullIndex.length > maxDisplayLength
    ? fullIndex.substring(0, 100) + '...' + fullIndex.substring(fullIndex.length - 100)
    : fullIndex;
  
  newIndexDisplay.textContent = truncated;
  newIndexDisplay.classList.add('index-clickable');
  newIndexDisplay.title = 'Click to download and view full index';
  
  // Add click handler
  newIndexDisplay.addEventListener('click', () => {
    // Create a blob with the full index
    const blob = new Blob([fullIndex], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    
    // Create a temporary download link
    const downloadLink = document.createElement('a');
    downloadLink.href = url;
    downloadLink.download = 'audio-index.txt';
    
    // Trigger download
    document.body.appendChild(downloadLink);
    downloadLink.click();
    document.body.removeChild(downloadLink);
    
    // Open in new tab
    const newWindow = window.open('', '_blank');
    if (newWindow) {
      newWindow.document.write(`
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
          <pre>${fullIndex}</pre>
        </body>
        </html>
      `);
      newWindow.document.close();
    }
    
    // Clean up the blob URL after a short delay
    setTimeout(() => URL.revokeObjectURL(url), 100);
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

  // Calculate and display position in library
  const positionDisplay = frag.get('#positionDisplay');
  if (positionDisplay && indexToShow) {
    try {
      const position = calculatePositionFromBase64(indexToShow);
      const roomDisplay = position.room === "" ? "0" : position.room;
      positionDisplay.innerHTML = `
        <div style="display: flex; gap: 16px; flex-wrap: wrap; font-size: 14px;">
          <span><strong>Room:</strong> ${roomDisplay}</span>
          <span><strong>Wall:</strong> ${position.wall}</span>
          <span><strong>Shelf:</strong> ${position.shelf}</span>
          <span><strong>Album:</strong> ${position.album}</span>
          <span><strong>Track:</strong> ${position.track}</span>
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
      cover.src = j.metadata.cover;
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
    const audioPlayer = frag.get('#audioPlayer');
    const downloadLink = frag.get('#downloadLink');
    if (audioPlayer) audioPlayer.src = url;
    if (downloadLink) downloadLink.href = url;
  }

  if (resultEl) resultEl.style.display = 'block';
}