// browse.js - client-side browsing UI for genres/artists/albums/tracks

const alphabet = [];
// order: a-z, A-Z, 0-9, -, _
for (let i = 0; i < 26; ++i) alphabet.push(String.fromCharCode(97 + i));
for (let i = 0; i < 26; ++i) alphabet.push(String.fromCharCode(65 + i));
for (let i = 0; i < 10; ++i) alphabet.push(String.fromCharCode(48 + i));
alphabet.push('-');
alphabet.push('_');

const ALPHABET_SIZE = alphabet.length; // 64
const TOKEN_LENGTH = 2; // show 2-char tokens (aa, ab,.. aA... a1... etc)
const TOTAL_TOKENS = Math.pow(ALPHABET_SIZE, TOKEN_LENGTH);
const PAGE_SIZE = 24;

let page = 0;
let filter = '';
let currentStage = 'genre';
const stages = ['genre', 'artist', 'album', 'track'];
const selection = { genre: null, artist: null, album: null, track: null };
// store last selected DOM element per stage so we can toggle CSS
const selectedEl = { genre: null, artist: null, album: null, track: null };

function indexToToken(idx) {
  const base = ALPHABET_SIZE;
  let n = idx;
  let out = '';
  for (let i = 0; i < TOKEN_LENGTH; ++i) {
    const digit = n % base;
    out = alphabet[digit] + out;
    n = Math.floor(n / base);
  }
  return out;
}

function escapeHtml(s) {
  if (!s) return '';
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function renderItems() {
  const container = document.getElementById('itemsContainer');
  container.innerHTML = '';
  const start = page * PAGE_SIZE;
  let count = 0;
  for (let i = start; i < TOTAL_TOKENS && count < PAGE_SIZE; ++i) {
    const t = indexToToken(i);
    if (filter && !t.toLowerCase().includes(filter.toLowerCase())) continue;
    const btn = document.createElement('button');
    btn.className = 'option';
    btn.textContent = t;
    btn.dataset.token = t;
  btn.addEventListener('click', () => onSelectToken(t, btn));
    // Disable items if trying to select a later stage before earlier selection
    if (stages.indexOf(currentStage) > 0) {
      // only enable when previous stage already selected
      const prev = stages[stages.indexOf(currentStage) - 1];
      if (!selection[prev]) btn.disabled = true;
    }
    container.appendChild(btn);
    count++;
  }
  updatePagerButtons();
}

function updatePagerButtons() {
  const prev = document.getElementById('prevPage');
  const next = document.getElementById('nextPage');
  prev.disabled = page <= 0;
  next.disabled = (page + 1) * PAGE_SIZE >= TOTAL_TOKENS;
}

function onSelectToken(token, btnEl) {
  selection[currentStage] = token;
  // toggle selected class
  const prev = selectedEl[currentStage];
  if (prev && prev !== btnEl) prev.classList.remove('selected');
  if (btnEl) btnEl.classList.add('selected');
  selectedEl[currentStage] = btnEl;
  // advance stage if possible
  const curIdx = stages.indexOf(currentStage);
  if (curIdx < stages.length - 1) {
    currentStage = stages[curIdx + 1];
    // reset page/filter for the next stage
    page = 0;
    filter = '';
    const fi = document.getElementById('filterInput');
    if (fi) fi.value = '';
    // when moving forward, clear any previously selected DOM for later stages
    stages.slice(curIdx + 1).forEach((s) => {
      if (selectedEl[s]) {
        selectedEl[s].classList.remove('selected');
        selectedEl[s] = null;
      }
    });
    renderItems();
  }
  updateBreadcrumb();
  updateGenerateButtonState();
}

function updateBreadcrumb() {
  const nav = document.getElementById('breadcrumb');
  nav.innerHTML = '';
  stages.forEach((s, idx) => {
    const span = document.createElement('span');
    span.style.marginRight = '8px';
    if (selection[s]) {
      const a = document.createElement('a');
      a.href = '#';
      a.textContent = selection[s];
      // highlight the crumb if it's the current stage
      if (s === currentStage) a.classList.add('active-stage');
      a.addEventListener('click', (ev) => {
        ev.preventDefault();
        // go back to this stage and clear later selections
        currentStage = s;
        stages.slice(idx + 1).forEach((later) => (selection[later] = null));
        // clear selected classes for later stages
        stages.slice(idx + 1).forEach((later) => {
          if (selectedEl[later]) {
            selectedEl[later].classList.remove('selected');
            selectedEl[later] = null;
          }
        });
        updateBreadcrumb();
        page = 0;
        renderItems();
        updateGenerateButtonState();
      });
      span.appendChild(a);
    } else {
      const txt = document.createElement('span');
      txt.textContent = s;
      txt.className = 'crumb-inactive';
      span.appendChild(txt);
    }
    nav.appendChild(span);
    if (idx < stages.length - 1) {
      const sep = document.createElement('span');
      sep.textContent = '›';
      sep.style.marginRight = '8px';
      nav.appendChild(sep);
    }
  });
}

function updateGenerateButtonState() {
  const btn = document.getElementById('generateBtn');
  const allSelected = stages.every((s) => !!selection[s]);
  btn.disabled = !allSelected;
  const status = document.getElementById('statusMsg');
  if (!allSelected) {
    const next = stages.find((s) => !selection[s]);
    status.textContent = `Select ${next}`;
  } else {
    status.textContent = 'Ready to generate';
  }
}

function init() {
  const fi = document.getElementById('filterInput');
  fi.addEventListener('input', (e) => {
    filter = e.target.value || '';
    page = 0;
    renderItems();
  });
  document.getElementById('prevPage').addEventListener('click', () => {
    if (page > 0) page--;
    renderItems();
  });
  document.getElementById('nextPage').addEventListener('click', () => {
    if ((page + 1) * PAGE_SIZE < TOTAL_TOKENS) page++;
    renderItems();
  });
  document.getElementById('generateBtn').addEventListener('click', generateWav);
  updateBreadcrumb();
  renderItems();
}

function toBase64Url(u8) {
  let binary = '';
  const len = u8.byteLength;
  for (let i = 0; i < len; ++i) binary += String.fromCharCode(u8[i]);
  let b64 = btoa(binary);
  // convert to url-safe (replace +/ and remove =)
  return b64.replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

async function generateWav() {
  const btn = document.getElementById('generateBtn');
  btn.disabled = true;
  const status = document.getElementById('statusMsg');
  status.textContent = 'Generating...';
  try {
    // Construct a simple payload embedding the selected tokens so the server's deterministic
    // reconstruction will be influenced by the selection. Use pipe separators to avoid ambiguity.
    const text = `${selection.genre}|${selection.artist}|${selection.album}|${selection.track}`;
    const enc = new TextEncoder().encode(text);
    const b64url = toBase64Url(enc);

    // Request metadata+wav as JSON so we can show cover art and metadata
    const resp = await fetch('/reconstruct?metadata=1', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
      body: JSON.stringify({ format: 'base64url', data: b64url }),
    });

    if (!resp.ok) {
      const j = await resp.json().catch(() => null);
      status.textContent = `Server error: ${resp.status}${j && j.error ? ' - ' + j.error : ''}`;
      btn.disabled = false;
      return;
    }

    const j = await resp.json().catch(() => null);
    if (!j) {
      status.textContent = 'Server returned unexpected response';
      btn.disabled = false;
      return;
    }

    // j.metadata: { genre, artist, album, track, cover }
    // j.wavBase64: base64 of WAV
    const rc = document.getElementById('resultContainer');
    rc.innerHTML = '';

    // cover may already be data:image/png;base64,...
    if (j.metadata && j.metadata.cover) {
      const img = document.createElement('img');
      img.src = j.metadata.cover;
      img.alt = `${j.metadata.track} cover`;
      img.style.width = '128px';
      img.style.height = '128px';
      img.style.borderRadius = '8px';
      img.style.objectFit = 'cover';
      img.style.display = 'inline-block';
      img.style.marginRight = '12px';
      rc.appendChild(img);
    }

    if (j.metadata) {
      const metaDiv = document.createElement('div');
      metaDiv.style.display = 'inline-block';
      metaDiv.style.verticalAlign = 'top';
      metaDiv.innerHTML = `
        <div style="font-weight:700">${escapeHtml(j.metadata.track || '')}</div>
        <div style="color:var(--muted)">${escapeHtml(j.metadata.artist || '')} — ${escapeHtml(j.metadata.album || '')}</div>
        <div style="margin-top:6px; color:var(--muted); font-size:13px">genre: ${escapeHtml(j.metadata.genre || '')}</div>
      `;
      rc.appendChild(metaDiv);
    }

    if (j.wavBase64) {
      const wavB64 = j.wavBase64;
      const byteChars = atob(wavB64);
      const len = byteChars.length;
      const bytes = new Uint8Array(len);
      for (let i = 0; i < len; ++i) bytes[i] = byteChars.charCodeAt(i);
      const blob = new Blob([bytes], { type: 'audio/wav' });
      const url = URL.createObjectURL(blob);

      const audio = document.createElement('audio');
      audio.controls = true;
      audio.src = url;
      audio.style.display = 'block';
      audio.style.marginTop = '12px';
      rc.appendChild(audio);

      const a = document.createElement('a');
      a.href = url;
      a.download = `${(j.metadata && j.metadata.genre) || selection.genre}_${(j.metadata && j.metadata.artist) || selection.artist}_${(j.metadata && j.metadata.album) || selection.album}_${(j.metadata && j.metadata.track) || selection.track}.wav`;
      a.textContent = 'Download .wav';
      a.style.display = 'inline-block';
      a.style.marginLeft = '12px';
      a.className = 'btn';
      rc.appendChild(a);
    }

    status.textContent = 'Generated';
  } catch (err) {
    console.error(err);
    status.textContent = 'Generation failed: ' + (err && err.message ? err.message : String(err));
  } finally {
    btn.disabled = false;
  }
}

document.addEventListener('DOMContentLoaded', init);
