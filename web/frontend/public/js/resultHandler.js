import { loadFragment } from './loadFragment.js';

let resultFrag = null;

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
 * Displays a JSON response containing audio metadata and WAV data.
 * @param {Object} j - JSON response object with metadata and wavBase64 properties
 * @param {string} [originalIndexB64] - Optional original index string to display
 */
export async function handleJsonResponse(j, originalIndexB64) {
  const frag = await ensureResultFrag();
  const indexDisplay = frag.get('#indexDisplay');
  const resultEl = frag.get('#result');

  // show index
  if (indexDisplay) indexDisplay.textContent = originalIndexB64 || j.indexBase64 || '';

  // metadata
  if (j.metadata) {
    const g = frag.get('#metaGenre');
    const a = frag.get('#metaArtist');
    const al = frag.get('#metaAlbum');
    const t = frag.get('#metaTrack');
    if (g) g.textContent = j.metadata.genre || '';
    if (a) a.textContent = j.metadata.artist || '';
    if (al) al.textContent = j.metadata.album || '';
    if (t) t.textContent = j.metadata.track || '';
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