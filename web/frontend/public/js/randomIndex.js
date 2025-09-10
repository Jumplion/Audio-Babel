import { toBase64 } from './utils.js';

export async function generateAndSend(regenBtn, handleJsonResponse, setLoading) {
  try {
    setLoading(true);
    const MIN_SIZE = 65536;
    const MAX_SIZE = 5242880;
    const size = Math.min(60 * 1024 * 1024, Math.floor(Math.random() * (MAX_SIZE - MIN_SIZE + 1)) + MIN_SIZE);
    const buf = new Uint8Array(size);
    if (window.crypto && window.crypto.getRandomValues) {
      const CHUNK = 65536;
      for (let i = 0; i < size; i += CHUNK) {
        const end = Math.min(i + CHUNK, size);
        window.crypto.getRandomValues(buf.subarray(i, end));
      }
    } else {
      for (let i = 0; i < size; ++i) buf[i] = Math.floor(Math.random() * 256);
    }
    const b64 = toBase64(buf.buffer);
    const resp = await fetch('/reconstruct?metadata=1', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
      body: JSON.stringify({ format: 'base64', data: b64 }),
    });
    if (!resp.ok) throw new Error('Server error ' + resp.status);
    const j = await resp.json();
    await handleJsonResponse(j, b64);
  } catch (err) {
    console.error(err);
    alert('Error: ' + String(err));
  } finally {
    setLoading(false);
  }
}
