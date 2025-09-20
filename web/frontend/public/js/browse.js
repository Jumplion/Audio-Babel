// browse.js - client-side browsing UI for genres/artists/albums/tracks

const alphabet = [];
// order: a-z, A-Z, 0-9, -, _
for (let i = 0; i < 26; ++i) alphabet.push(String.fromCharCode(97 + i));
for (let i = 0; i < 26; ++i) alphabet.push(String.fromCharCode(65 + i));
for (let i = 0; i < 10; ++i) alphabet.push(String.fromCharCode(48 + i));
alphabet.push('-');
alphabet.push('_');

const ALPHABET_SIZE = BigInt(alphabet.length); // 64
// browsing configuration
// NOTE: tokens are now allowed to be arbitrarily long. We keep a soft-length cap
// to warn users when tokens exceed a sensible length for browsing (see SOFT_LENGTH_CAP).
const SOFT_LENGTH_CAP = 6; // soft warning threshold (old hard cap)
let pageSize = 50; // fixed number of entries per page

// (removed old tokenLength-based total function)

let page = 0n; // BigInt page index (0-based)
let filter = '';
let currentStage = 'genre';
const stages = ['genre', 'artist', 'album', 'track'];
const selection = { genre: null, artist: null, album: null, track: null };
// store last selected DOM element per stage so we can toggle CSS
const selectedEl = { genre: null, artist: null, album: null, track: null };

// Generate token for a variable-length enumeration where tokens are
// ordered by increasing length: all length-1 tokens, then length-2, etc.
// Enumeration is unbounded; longer tokens are supported but a soft-cap
// will warn users when lengths exceed a sensible threshold.
function indexToVariableToken(indexBig) {
  // Map a 0-based integer index into the ordered variable-length tokens
  // (all length-1 strings, then length-2, ...). This implementation no longer
  // imposes a hard maximum length.
  let n = BigInt(indexBig);
  const base = ALPHABET_SIZE;
  // find the length by subtracting counts until n fits into the current length
  let len = 1;
  while (true) {
    const count = base ** BigInt(len);
    if (n < count) break;
    n -= count;
    len++;
  }
  // now n is the local index within strings of length `len`
  let out = '';
  let local = n;
  for (let i = 0; i < len; ++i) {
    const digit = Number(local % base);
    out = alphabet[digit] + out;
    local = local / base;
  }
  return out;
}

function totalTokensBig() {
  // unlimited token space; return `null` to indicate an infinite/unknown total.
  return null;
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

// Small DOM helper: $(id) -> element or null
function $(id) {
  return document.getElementById(id);
}

// If total is finite, clamp page to the last page. If total is null (infinite) return page unchanged.
function clampPageForTotal(p) {
  const total = totalTokensBig();
  if (total === null) return p;
  const totalPages = (total + BigInt(pageSize) - 1n) / BigInt(pageSize);
  if (p < 0n) return 0n;
  if (p >= totalPages) return totalPages - 1n;
  return p;
}

// Create or return the softcap warning element. Keeps creation logic in one place.
function createOrGetSoftcapWarning(container) {
  const warnElId = 'softcapWarning';
  let warnEl = $(warnElId);
  if (!warnEl) {
    warnEl = document.createElement('div');
    warnEl.id = warnElId;
    warnEl.style.margin = '8px 0';
    warnEl.style.padding = '8px';
    warnEl.style.border = '1px solid var(--muted)';
    warnEl.style.borderRadius = '6px';
    warnEl.style.background = 'var(--bg)';
    warnEl.style.color = 'var(--muted)';
    warnEl.style.display = 'none';
    container.parentNode && container.parentNode.insertBefore(warnEl, container);
  }
  return warnEl;
}

function renderItems() {
  const container = document.getElementById('itemsContainer');
  if (!container) {
    console.warn('browse.js: itemsContainer not found');
    return;
  }
  container.innerHTML = '';
  const total = totalTokensBig();
  const start = page * BigInt(pageSize);
  // when total is null the space is infinite; we'll iterate until we've
  // rendered `pageSize` items or hit a safety attempt limit if a filter
  // prevents matches.
  console.debug('browse.renderItems', { page: page.toString(), pageSize, total: total === null ? 'infinite' : total.toString() });
  let count = 0;
  let attempts = 0n;
  const maxAttempts = BigInt(pageSize) * 200n + 1000n; // safety when filter is restrictive
  for (let i = start; (total ? i <= (total - 1n) : true) && count < pageSize; i = i + 1n) {
    if (attempts > maxAttempts) break;
    attempts++;
    const t = indexToVariableToken(i);
    // In non-prefix mode we may still have a filter (substring). In prefix mode, filter is the prefix and always matches.
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
  console.debug('browse.renderItems done', { rendered: count, start: start.toString(), total: total === null ? 'infinite' : total.toString() });
  // show a soft-cap UI warning when token lengths exceed the old hard cap
  try {
    const longestShown = Array.from(container.querySelectorAll('button.option')).reduce((m, b) => Math.max(m, (b.textContent || '').length), 0);
    const dismissed = localStorage.getItem('softcapWarningDismissed') === '1';
    const warnEl = createOrGetSoftcapWarning(container);
    if (!dismissed && longestShown > SOFT_LENGTH_CAP) {
      warnEl.style.display = 'block';
      warnEl.innerHTML = `Warning: some tokens exceed ${SOFT_LENGTH_CAP} characters which may make browsing slow or hard to read. <button id="dismissSoftcap" class="btn" style="margin-left:12px">Dismiss</button>`;
      const dBtn = $('dismissSoftcap');
      if (dBtn) dBtn.addEventListener('click', () => {
        localStorage.setItem('softcapWarningDismissed', '1');
        warnEl.style.display = 'none';
      });
    } else {
      warnEl.style.display = 'none';
    }
  } catch (e) {
    // non-fatal UI enhancement; ignore on error
    console.error('softcap warning update failed', e);
  }
  updatePagerButtons();
  updatePageInfo();
}

function updatePagerButtons() {
  const prev = document.getElementById('prevPage');
  const next = document.getElementById('nextPage');
  prev.disabled = page <= 0n;
  const total = totalTokensBig();
  if (!next) return;
  if (total === null) {
    // infinite space: next is always enabled
    next.disabled = false;
  } else {
    const nextStart = (page + 1n) * BigInt(pageSize);
    next.disabled = nextStart >= total;
  }
}

function updatePageInfo() {
  const info = document.getElementById('pageInfo');
  const infoTop = document.getElementById('pageInfoTop');
  // only show current page number (hide total pages)
  const text = `Page ${page + 1n}`;
  if (info) info.textContent = text;
  if (infoTop) infoTop.textContent = text;
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
    page = 0n;
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
  // auto-scroll to top of the items list when a selection is made
  try {
    const items = document.getElementById('itemsContainer');
    if (items) items.scrollIntoView({ behavior: 'smooth', block: 'start' });
  } catch (e) {
    console.error('browse.js: scrollIntoView failed', e);
  }
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
  page = 0n;
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
  if (fi) {
    fi.addEventListener('input', (e) => {
      filter = e.target.value || '';
      page = 0n;
      renderItems();
    });
  }
  // wire controls (uses helper to reduce duplication between top/bottom controls)
  function addPageControls({ prevId, nextId, goId, inputId, allowInfiniteNext }) {
    const prev = $(prevId);
    const next = $(nextId);
    const input = $(inputId);
    const go = $(goId);
    if (prev) prev.addEventListener('click', () => { if (page > 0n) { page = page - 1n; renderItems(); } });
    if (next) next.addEventListener('click', () => {
      const total = totalTokensBig();
      if (total === null && allowInfiniteNext) { page = page + 1n; }
      else if (total !== null) {
        const nextStart = (page + 1n) * BigInt(pageSize);
        if (nextStart < total) page = page + 1n;
      }
      renderItems();
    });
    if (go && input) {
      go.addEventListener('click', () => {
        const v = input.value && input.value.trim();
        if (!v) return;
        try {
          const pn = BigInt(v);
          if (pn <= 0n) return;
          const target = pn - 1n;
          page = clampPageForTotal(target);
          renderItems();
        } catch (e) {
          // invalid input; ignore
        }
      });
    }
  }

  addPageControls({ prevId: 'prevPage', nextId: 'nextPage', goId: 'goPage', inputId: 'pageInput', allowInfiniteNext: false });
  addPageControls({ prevId: 'prevPageTop', nextId: 'nextPageTop', goId: 'goPageTop', inputId: 'pageInputTop', allowInfiniteNext: true });
  // pageSize is fixed at 50 per design
  const genBtn = document.getElementById('generateBtn');
  if (genBtn) genBtn.addEventListener('click', generateWav);
  updateBreadcrumb();
  try { renderItems(); } catch (e) { console.error('browse.js: renderItems failed', e); }
  // ensure the generate button reflects current selections on load
  try { updateGenerateButtonState(); } catch (e) { console.error('browse.js: updateGenerateButtonState failed', e); }
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

console.info('browse.js loaded');
document.addEventListener('DOMContentLoaded', () => {
  console.info('browse.js DOMContentLoaded');
  try { init(); } catch (e) { console.error('browse.init error', e); }
});
