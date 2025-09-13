import { toBase64 } from './utils.js';

function isValidIndexString(s) {
  return /^[A-Za-z0-9_-]*$/.test(s);
}

export async function generateFromIndex(inputEl, btnEl, handleJsonResponse, setLoading) {
  const raw = inputEl.value || '';
  if (!isValidIndexString(raw)) {
    alert('Invalid characters in index. Only A-Z, a-z, 0-9, - and _ are allowed.');
    return;
  }
  // convert the string to bytes (ASCII/UTF-8 single-byte for allowed chars)
  const buf = new Uint8Array(raw.length);
  for (let i = 0; i < raw.length; ++i) buf[i] = raw.charCodeAt(i) & 0xff;
  const b64 = toBase64(buf.buffer);
  try {
    setLoading(true);
    const resp = await fetch('/reconstruct?metadata=1', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
      body: JSON.stringify({ format: 'base64', data: b64 }),
    });
    if (!resp.ok) throw new Error('Server error ' + resp.status);
    const j = await resp.json();
    await handleJsonResponse(j, b64);
  } catch (e) {
    console.error(e);
    alert('Error: ' + String(e));
  } finally {
    setLoading(false);
  }
}

export function attachBrowseInputFilter(inputEl) {
  if (!inputEl) return;
  const allowed = /[A-Za-z0-9_-]/;

  function autosize() {
    try {
      inputEl.style.height = 'auto';
      const h = inputEl.scrollHeight;
      inputEl.style.height = Math.max(24, h) + 'px';
    } catch (e) {
      /* ignore */
    }
  }

  // prevent invalid key input
  inputEl.addEventListener('keypress', (e) => {
    const ch = String.fromCharCode(e.charCode || e.which || 0);
    if (!allowed.test(ch)) e.preventDefault();
  });

  // sanitize pasted content
  inputEl.addEventListener('paste', (e) => {
    try {
      const txt = (e.clipboardData || window.clipboardData).getData('text') || '';
      const filtered = txt.split('').filter((c) => allowed.test(c)).join('');
      if (filtered !== txt) {
        e.preventDefault();
        // insert filtered text at caret
  const start = inputEl.selectionStart || 0;
  const end = inputEl.selectionEnd || 0;
  const v = inputEl.value || '';
  inputEl.value = v.slice(0, start) + filtered + v.slice(end);
  const pos = start + filtered.length;
  inputEl.setSelectionRange(pos, pos);
  autosize();
      }
    } catch (err) {
      // fallback: do nothing
    }
  });

  // sanitize programmatic input (e.g., drag/drop) on input event
  inputEl.addEventListener('input', () => {
    const v = inputEl.value || '';
    const filtered = v.split('').filter((c) => allowed.test(c)).join('');
    if (filtered !== v) {
      const pos = inputEl.selectionStart || filtered.length;
      inputEl.value = filtered;
      inputEl.setSelectionRange(Math.max(0, pos - 1), Math.max(0, pos - 1));
    }
    autosize();
  });
  // initial size
  autosize();
}
