import { loadFragment } from './loadFragment.js';

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
 * Displays a JSON response containing audio metadata and WAV data.
 * @param {Object} j - JSON response object with metadata and wavBase64 properties
 * @param {string} [originalIndexB64] - Optional original index string to display
 */
export async function handleJsonResponse(j, originalIndexB64) {
  const frag = await ensureResultFrag();
  const indexDisplay = frag.get('#indexDisplay');
  const resultEl = frag.get('#result');

  // show index (truncated if very long)
  const indexToShow = originalIndexB64 || j.indexBase64 || '';
  if (indexDisplay) {
    indexDisplay.textContent = indexToShow;
    indexDisplay.title = indexToShow; // Full index on hover
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

  if (resultEl) resultEl.style.display = '';
}