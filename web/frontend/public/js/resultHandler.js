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
  const similarContainer = frag.get('#similarContainer');
  const similarList = frag.get('#similarList');

  // show index
  if (indexDisplay) indexDisplay.textContent = originalIndexB64 || j.indexBase64 || '';

  // generate initial similar variants if we have an index base64 available
  try {
    const rawIndexB64 = originalIndexB64 || j.indexBase64 || '';
    if (rawIndexB64 && similarList) {
      const variants = generateSimilarVariants(rawIndexB64, 6);
      renderSimilarList(variants, similarList, similarContainer);
    } else if (similarContainer) {
      similarContainer.style.display = 'none';
    }
  } catch (e) {
    console.warn('Failed to generate similar indices', e);
  }

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

    // Add the track to the player playlist but do not auto-play.
    const track = { wavUrl: url, index: originalIndexB64 || j.indexBase64, metadata: j.metadata };
    await waitForPlayerAndAdd(track);

    // After primary audio metadata is available, create a "next power-of-two"
    // duration variant by embedding the original index into a larger buffer.
    try {
      const rawIndexB64 = originalIndexB64 || j.indexBase64 || '';
      if (rawIndexB64 && similarList && audioPlayer) {
        const tryAddPow2 = async () => {
          try {
            const dur = audioPlayer.duration;
            if (!isFinite(dur) || dur <= 0) return;
            // next power of two strictly greater than current duration
            let nextPow = Math.pow(2, Math.ceil(Math.log2(dur)));
            if (nextPow <= dur) nextPow *= 2;
            // compute rough target byte-size assuming linear scaling between index bytes and audio duration
            const baseBytes = b64ToBytes(rawIndexB64).length;
            const targetBytes = Math.max(baseBytes + 1, Math.round((baseBytes * nextPow) / dur));
            if (targetBytes <= baseBytes) return;
            // build buffer with random bytes and embed base at a random offset
            const out = randBytes(targetBytes);
            const base = b64ToBytes(rawIndexB64);
            const maxOffset = targetBytes - base.length;
            const offset = Math.floor(Math.random() * (maxOffset + 1));
            out.set(base, offset);
            const variantB64 = bytesToB64(out);
            // prepend or insert if unique
            const current = Array.from(new Set(generateSimilarVariants(rawIndexB64, 6)));
            if (!current.includes(variantB64)) {
              current.unshift(variantB64);
            }
            renderSimilarList(current.slice(0, 6), similarList, similarContainer);
          } catch (err) {
            console.warn('Failed to add pow2 variant', err);
          }
        };

        if (isFinite(audioPlayer.duration) && audioPlayer.duration > 0) {
          await tryAddPow2();
        } else {
          const onLoaded = async () => {
            audioPlayer.removeEventListener('loadedmetadata', onLoaded);
            await tryAddPow2();
          };
          audioPlayer.addEventListener('loadedmetadata', onLoaded);
        }
      }
    } catch (e) {
      console.warn('Failed to generate pow2 similar index', e);
    }
  }

  if (resultEl) resultEl.style.display = '';
}

function b64ToBytes(b64) {
  const bin = atob(b64);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; ++i) out[i] = bin.charCodeAt(i);
  return out;
}

function bytesToB64(bytes) {
  let s = '';
  for (let i = 0; i < bytes.length; ++i) s += String.fromCharCode(bytes[i]);
  return btoa(s);
}

function randBytes(n) {
  const b = new Uint8Array(n);
  if (window.crypto && window.crypto.getRandomValues) window.crypto.getRandomValues(b);
  else for (let i = 0; i < n; ++i) b[i] = Math.floor(Math.random() * 256);
  return b;
}

function generateSimilarVariants(indexB64, limit = 6) {
  // variants include: original, prefix random 1-3 chars + original, original + suffix random, and wrapped
  const base = b64ToBytes(indexB64);
  const variants = [];
  // include original as first
  variants.push(indexB64);
  const tries = Math.max(limit, 6);
  while (variants.length < limit && variants.length < tries) {
    const mode = variants.length % 3;
    if (mode === 0) {
      // prefix random 1-3 bytes
      const n = 1 + (variants.length % 3);
      const p = randBytes(n);
      const out = new Uint8Array(p.length + base.length);
      out.set(p, 0);
      out.set(base, p.length);
      variants.push(bytesToB64(out));
    } else if (mode === 1) {
      // suffix random
      const n = 1 + (variants.length % 4);
      const s = randBytes(n);
      const out = new Uint8Array(base.length + s.length);
      out.set(base, 0);
      out.set(s, base.length);
      variants.push(bytesToB64(out));
    } else {
      // wrapped: random prefix + original + random suffix
      const n1 = 1 + ((variants.length + 1) % 3);
      const n2 = 1 + ((variants.length + 2) % 3);
      const p = randBytes(n1);
      const s = randBytes(n2);
      const out = new Uint8Array(p.length + base.length + s.length);
      out.set(p, 0);
      out.set(base, p.length);
      out.set(s, p.length + base.length);
      variants.push(bytesToB64(out));
    }
  }
  // ensure unique
  return Array.from(new Set(variants)).slice(0, limit);
}

function renderSimilarList(variants, listEl, containerEl) {
  if (!listEl) return;
  listEl.innerHTML = '';
  variants.forEach((b64) => {
    const li = document.createElement('li');
    const btn = document.createElement('button');
    btn.className = 'btn';
    btn.textContent = shortenB64(b64);
    btn.addEventListener('click', async () => {
      // fetch reconstructed wav for this variant and show it in a small player
      try {
        btn.disabled = true;
        const resp = await fetch('/reconstruct?metadata=1', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
          body: JSON.stringify({ format: 'base64', data: b64 }),
        });
        if (!resp.ok) throw new Error('Server error ' + resp.status);
        const j = await resp.json();
        // show a temporary audio element for this variant
        const tmpId = 'similar-audio-' + Math.random().toString(36).slice(2, 8);
        const existing = document.getElementById(tmpId);
        if (existing) existing.remove();
        if (j.wavBase64) {
          const bytes = atob(j.wavBase64);
          const ab = new Uint8Array(bytes.length);
          for (let i = 0; i < bytes.length; ++i) ab[i] = bytes.charCodeAt(i);
          const blob = new Blob([ab], { type: 'audio/wav' });
          const url = URL.createObjectURL(blob);
          // create small player below the similar list
          let player = document.getElementById('similar-preview');
          if (!player) {
            player = document.createElement('div');
            player.id = 'similar-preview';
            player.style.marginTop = '8px';
            containerEl.parentNode.insertBefore(player, containerEl.nextSibling);
          }
          player.innerHTML = '';
          const a = document.createElement('audio');
          a.controls = true;
          a.src = url;
          a.style.width = '100%';
          player.appendChild(a);
        }
      } catch (e) {
        console.error(e);
        alert('Failed to fetch variant: ' + e.message);
      } finally {
        btn.disabled = false;
      }
    });
    li.appendChild(btn);
    listEl.appendChild(li);
  });
  if (containerEl) containerEl.style.display = variants.length ? '' : 'none';
}

function shortenB64(b64) {
  if (!b64) return '';
  return b64.length > 12 ? b64.slice(0, 8) + '...' + b64.slice(-4) : b64;
}

async function waitForPlayerAndAdd(track, timeoutMs = 3000) {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    if (window.__SOTB_PLAYER && typeof window.__SOTB_PLAYER.add === 'function') {
      try {
        window.__SOTB_PLAYER.add(track);
        return true;
      } catch (e) {
        console.warn('Player add failed', e);
        return false;
      }
    }
    // small backoff
    await new Promise((r) => setTimeout(r, 100));
  }
  // timed out: persist pending track to sessionStorage for the player to pick up later
  try {
    const key = 'sotb.pending_tracks';
    const cur = JSON.parse(sessionStorage.getItem(key) || '[]');
    cur.push(track);
    sessionStorage.setItem(key, JSON.stringify(cur));
    console.warn('Player not ready; saved pending track to sessionStorage');
  } catch (e) {
    console.warn('Failed to persist pending track', e);
  }
  return false;
}
