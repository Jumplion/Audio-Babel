import { loadFragment } from './loadFragment.js';

let resultFrag = null;
export async function ensureResultFrag() {
  if (!resultFrag) resultFrag = await loadFragment('#resultContainer', './components/result.html');
  return resultFrag;
}

export async function handleJsonResponse(j, originalIndexB64) {
  const frag = await ensureResultFrag();
  const indexDisplay = frag.get('#indexDisplay');
  const resultEl = frag.get('#result');
  if (indexDisplay) indexDisplay.textContent = originalIndexB64 || j.indexBase64 || '';
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
    // Add the track to the player playlist but do not auto-play.
    if (window.__SOTB_PLAYER && typeof window.__SOTB_PLAYER.add === 'function')
      window.__SOTB_PLAYER.add({ wavUrl: url, index: originalIndexB64 || j.indexBase64, metadata: j.metadata });
  }
  if (resultEl) resultEl.style.display = '';
}
