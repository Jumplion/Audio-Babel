import express, { json } from 'express';
import cors from 'cors';
import morgan from 'morgan';
import { fileURLToPath } from 'url';
// POST /reconstruct
// Body (application/json): { format: 'base64', data: string }
import { tmpdir } from 'os';
import { randomBytes } from 'crypto';
import { writeFileSync, unlinkSync, createReadStream, existsSync, statSync, accessSync, readFileSync } from 'fs';
import { spawn } from 'child_process';
import path from 'path';
import zlib from 'zlib';

const app = express();
app.use(cors());
app.use(morgan('dev'));
app.use(json({ limit: '50mb' }));

// Resolve __dirname for ES modules and serve static frontend (public)
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const frontendPublic = path.resolve(path.join(__dirname, '..', '..', 'frontend', 'public'));
  try {
    accessSync(frontendPublic);
    console.log('Serving static frontend from', frontendPublic);
  } catch (e) {
    console.log('Frontend public not found at', frontendPublic);
  }
app.use(express.static(frontendPublic));

// Serve index at root explicitly
app.get('/', (req, res) => {
  res.sendFile(path.join(frontendPublic, 'index.html'));
});

function normalizeBase64(s) {
  // Convert base64url to standard base64 and add padding
  s = s.replace(/-/g, '+').replace(/_/g, '/');
  const pad = s.length % 4;
  if (pad === 2) s += '==';
  else if (pad === 3) s += '=';
  else if (pad !== 0) throw new Error('Invalid base64/base64url string length');
  return s;
}

app.get('/health', (req, res) => {
  res.json({ status: 'ok' });
});

// Limits (tunable)
// Allow larger index payloads so reconstructed WAVs can be ~6 minutes long.
// ~60 MB chosen to permit indexes that reconstruct to ~6 minutes at typical settings.
const MAX_INDEX_BYTES = 60 * 1024 * 1024; // ~60 MB max raw index input (~6 minutes)
const MAX_WAV_BYTES = 100 * 1024 * 1024; // 100 MB max reconstructed WAV
const CHILD_TIMEOUT_MS = 20 * 1000; // 20s timeout for reconstruction

app.post('/reconstruct', async (req, res) => {
  const { format, data } = req.body || {};
  if (!format || typeof data !== 'string') {
    return res.status(400).json({ error: 'Expected JSON { format: string, data: string }' });
  }

  if (!['base64', 'base64url'].includes(format.toLowerCase())) {
    return res
      .status(400)
      .json({
        error: 'This endpoint accepts only base64 or base64url formats for direct reconstruction',
      });
  }

  let buf;
  try {
    buf = Buffer.from(normalizeBase64(data), 'base64');
  } catch (err) {
    return res
      .status(400)
      .json({
        error: 'Failed to decode base64 input',
        message: String(err && err.message ? err.message : err),
      });
  }

  // Basic validation: non-empty and size limit
  if (!buf || buf.length === 0) {
    return res.status(400).json({ error: 'Decoded input is empty' });
  }
  if (buf.length > MAX_INDEX_BYTES) {
    return res.status(413).json({ error: 'Input too large', maxBytes: MAX_INDEX_BYTES });
  }

  // Preserve raw decoded payload so we can derive metadata deterministically from it
  const rawPayload = Buffer.from(buf);

  // Always append a default 16-byte header to ensure a predictable header is present.
  // Default header: 44100 Hz, 16-bit, 1 channel, 0 frames
  const defaultSampleRate = 44100;
  const defaultBitDepth = 16;
  const defaultNumChannels = 1;

  // Default to 0 frames so the decoder must infer the frame count from the payload bytes
  const defaultNumFrames = 0n;
  const hdr = Buffer.alloc(16);
  hdr.writeUInt32BE(defaultSampleRate, 0);
  hdr.writeUInt16BE(defaultBitDepth, 4);
  hdr.writeUInt16BE(defaultNumChannels, 6);
  if (typeof hdr.writeBigUInt64BE === 'function') {
    hdr.writeBigUInt64BE(defaultNumFrames, 8);
  } else {
    let n = defaultNumFrames;
    for (let i = 15; i >= 8; --i) {
      hdr[i] = Number(n & 0xffn);
      n >>= 8n;
    }
  }

  // Append unconditionally; the reconstruction code will read the LSB 16 bytes as header.
  buf = Buffer.concat([buf, hdr]);

  try {
    const headerBuf = buf.slice(buf.length - 16);
    const sampleRate = headerBuf.readUInt32BE(0);
    const bitDepth = headerBuf.readUInt16BE(4);
    const numChannels = headerBuf.readUInt16BE(6);
    let numFrames = null;
    if (typeof headerBuf.readBigUInt64BE === 'function') {
      numFrames = Number(headerBuf.readBigUInt64BE(8));
    } else {
      // Fallback for older Node: manual BigInt parse
      let n = 0n;
      for (let i = 8; i < 16; ++i) n = (n << 8n) | BigInt(headerBuf[i]);
      numFrames = Number(n);
    }

    console.log(
      `Decoded header -> sampleRate=${sampleRate} bitDepth=${bitDepth} numChannels=${numChannels} numFrames=${numFrames}`
    );

    const ALLOWED_BIT_DEPTHS = new Set([8, 16, 24, 32]);
    if (!ALLOWED_BIT_DEPTHS.has(bitDepth)) {
      return res
        .status(422)
        .json({
          error: 'Unsupported bit depth in index header',
          bitDepth,
          allowed: Array.from(ALLOWED_BIT_DEPTHS),
        });
    }
  } catch (err) {
    console.error('Header parse error', err);
    return res
      .status(400)
      .json({
        error: 'Failed to parse header from decoded input',
        message: String(err && err.message ? err.message : err),
      });
  }

  // Helper: derive deterministic metadata from payload bytes and produce a PNG cover.
  // The PNG is deterministically generated from the payload bytes so identical indexes produce identical covers.
  function deriveMetadata(payload) {
    const b = payload || Buffer.alloc(0);
    function token(offset, len) {
      let s = '';
      for (let i = 0; i < len; ++i) {
        const v = (offset + i < b.length) ? b[offset + i] : 0;
        const r = v % 36;
        s += (r < 10) ? String.fromCharCode(48 + (r)) : String.fromCharCode(97 + (r - 10));
      }
      return s;
    }
    const genre = token(0, 6);
    const artist = token(6, 8);
    const album = token(14, 8);
    const track = token(22, 6);

    // Build a deterministic PNG from the payload bytes.
    function crc32(buf) {
      const table = crc32.table || (crc32.table = (function() {
        const t = new Uint32Array(256);
        for (let i = 0; i < 256; ++i) {
          let c = i;
          for (let k = 0; k < 8; ++k) c = ((c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1));
          t[i] = c >>> 0;
        }
        return t;
      })());
      let crc = 0xffffffff;
      for (let i = 0; i < buf.length; ++i) {
        crc = (crc >>> 8) ^ table[(crc ^ buf[i]) & 0xff];
      }
      return (crc ^ 0xffffffff) >>> 0;
    }

    function makePng(payloadBuf, size = 256) {
      const width = size;
      const height = size;
      const bytesPerPixel = 3; // RGB
      const rowSize = width * bytesPerPixel;
      const raw = Buffer.alloc((rowSize + 1) * height);
      const plen = payloadBuf.length || 1;
      // Fill pixels deterministically from payload repeating as needed
      for (let y = 0; y < height; ++y) {
        const rowStart = y * (rowSize + 1);
        raw[rowStart] = 0; // no filter
        for (let x = 0; x < width; ++x) {
          const p = y * width + x;
          const base = (p * 3) % plen;
          const r = payloadBuf.length ? payloadBuf[base % plen] : 0;
          const g = payloadBuf.length ? payloadBuf[(base + 1) % plen] : 0;
          const bch = payloadBuf.length ? payloadBuf[(base + 2) % plen] : 0;
          const off = rowStart + 1 + x * 3;
          raw[off] = r;
          raw[off + 1] = g;
          raw[off + 2] = bch;
        }
      }

      const idat = zlib.deflateSync(raw);

      function chunk(type, data) {
        const typeBuf = Buffer.from(type, 'ascii');
        const len = Buffer.alloc(4);
        len.writeUInt32BE(data.length, 0);
        const crcBuf = Buffer.alloc(4);
        const crc = crc32(Buffer.concat([typeBuf, data]));
        crcBuf.writeUInt32BE(crc, 0);
        return Buffer.concat([len, typeBuf, data, crcBuf]);
      }

      const sig = Buffer.from([137,80,78,71,13,10,26,10]);
      const ihdr = Buffer.alloc(13);
      ihdr.writeUInt32BE(width, 0);
      ihdr.writeUInt32BE(height, 4);
      ihdr[8] = 8; // bit depth
      ihdr[9] = 2; // color type RGB
      ihdr[10] = 0; // compression
      ihdr[11] = 0; // filter
      ihdr[12] = 0; // interlace

      const png = Buffer.concat([sig, chunk('IHDR', ihdr), chunk('IDAT', idat), chunk('IEND', Buffer.alloc(0))]);
      return png;
    }

    const pngBuf = makePng(b, 256);
    const coverBase64 = pngBuf.toString('base64');
    return { genre, artist, album, track, coverBase64 };
  }

  const derivedMetadata = deriveMetadata(rawPayload);

  // Write temp input file and choose temp output path
  const tmp = tmpdir();
  const rnd = randomBytes(8).toString('hex');
  const inPath = path.join(tmp, `sotb_index_${rnd}.bin`);
  const outPath = path.join(tmp, `sotb_recon_${rnd}.wav`);
  try {
    writeFileSync(inPath, buf);
  } catch (err) {
    console.error('failed to write temp file', err);
    return res.status(500).json({ error: 'Failed to write temp file' });
  }

  // server.js lives at <repo>/web/server/src, repo root is three levels up
  const repoRoot = path.resolve(__dirname, '..', '..', '..');
  const candidates = [
    path.join(repoRoot, 'build', 'Debug', 'reconstruct_cli.exe'),
    path.join(repoRoot, 'build', 'reconstruct_cli.exe'),
    path.join(repoRoot, 'build', 'reconstruct_cli'),
    // Also allow the build folder next to web, in case build was created from repo root
    path.join(repoRoot, '..', 'build', 'reconstruct_cli.exe'),
    path.join(repoRoot, '..', 'build', 'reconstruct_cli'),
    path.join(process.cwd(), 'build', 'reconstruct_cli.exe'),
    path.join(process.cwd(), 'build', 'reconstruct_cli'),
    path.join(repoRoot, 'reconstruct_cli.exe'),
    path.join(repoRoot, 'reconstruct_cli'),
  ];

  let cliPath = null;
  for (const c of candidates) {
    try {
      if (c && existsSync(c)) {
        cliPath = c;
        break;
      }
    } catch (e) {}
  }
  if (cliPath) console.log('Using reconstruct_cli at:', cliPath);
  // fallback to assuming it's on PATH
  if (!cliPath) cliPath = 'reconstruct_cli';
  const child = spawn(cliPath, [inPath, outPath], { stdio: ['ignore', 'pipe', 'pipe'] });
  let stderr = '';
  child.stderr.on('data', (d) => {
    stderr += d.toString();
  });

  let timedOut = false;
  const killTimer = setTimeout(() => {
    timedOut = true;
    try {
      child.kill('SIGKILL');
    } catch (e) {}
  }, CHILD_TIMEOUT_MS);

  child.on('error', (err) => {
    clearTimeout(killTimer);
    console.error('Failed to spawn reconstruct_cli', err);
    try {
      unlinkSync(inPath);
    } catch (_) {}
    return res
      .status(500)
      .json({
        error: 'Failed to run recon CLI',
        message: String(err && err.message ? err.message : err),
      });
  });

  // Handle child process close
  child.on('close', (code, signal) => {
    clearTimeout(killTimer);
    if (timedOut) {
      console.error('reconstruct_cli timed out', CHILD_TIMEOUT_MS);
      try {
        unlinkSync(inPath);
      } catch (_) {}
      try {
        unlinkSync(outPath);
      } catch (_) {}
      return res
        .status(504)
        .json({ error: 'Reconstruction timed out', timeoutMs: CHILD_TIMEOUT_MS });
    }

    if (code !== 0) {
      console.error('reconstruct_cli failed', code, stderr);
      try {
        unlinkSync(inPath);
      } catch (_) {}
      try {
        unlinkSync(outPath);
      } catch (_) {}
      return res.status(500).json({ error: 'Reconstruction failed', code, message: stderr });
    }

    // Inspect output size before streaming
    try {
      if (!existsSync(outPath)) {
        throw new Error('Output file not found');
      }
      const st = statSync(outPath);
      if (st.size > MAX_WAV_BYTES) {
        try {
          unlinkSync(inPath);
        } catch (_) {}
        try {
          unlinkSync(outPath);
        } catch (_) {}
        return res
          .status(413)
          .json({ error: 'Reconstructed WAV too large', maxBytes: MAX_WAV_BYTES });
      }
    } catch (err) {
      console.error('Failed to stat output file', err);
      try {
        unlinkSync(inPath);
      } catch (_) {}
      try {
        unlinkSync(outPath);
      } catch (_) {}
      return res
        .status(500)
        .json({
          error: 'Failed to read reconstructed output',
          message: String(err && err.message ? err.message : err),
        });
    }

    // If client requested metadata (query param or JSON accept), return JSON with metadata + base64 WAV
    const wantsMetadata = req.query && (req.query.metadata === '1' || req.query.metadata === 'true' || (req.headers.accept && req.headers.accept.indexOf('application/json') !== -1));
    if (wantsMetadata) {
      try {
  const wavBuf = readFileSync(outPath);
        const wavB64 = wavBuf.toString('base64');
        const meta = {
          genre: derivedMetadata.genre,
          artist: derivedMetadata.artist,
          album: derivedMetadata.album,
          track: derivedMetadata.track,
          cover: `data:image/png;base64,${derivedMetadata.coverBase64}`,
        };
        try {
          unlinkSync(inPath);
        } catch (_) {}
        try {
          unlinkSync(outPath);
        } catch (_) {}
        return res.json({ metadata: meta, wavBase64: wavB64 });
      } catch (err) {
        console.error('Failed to read WAV for json response', err);
        try {
          unlinkSync(inPath);
        } catch (_) {}
        try {
          unlinkSync(outPath);
        } catch (_) {}
        return res.status(500).json({ error: 'Failed to read reconstructed output', message: String(err && err.message ? err.message : err) });
      }
    }

    // Stream the WAV back (legacy behavior)
    res.setHeader('Content-Type', 'audio/wav');
    res.setHeader('Content-Disposition', 'attachment; filename="reconstructed.wav"');
    const stream = createReadStream(outPath);
    stream.on('end', () => {
      try {
        unlinkSync(inPath);
      } catch (_) {}
      try {
        unlinkSync(outPath);
      } catch (_) {}
    });
    stream.on('error', (err) => {
      console.error('Stream error', err);
      try {
        unlinkSync(inPath);
      } catch (_) {}
      try {
        unlinkSync(outPath);
      } catch (_) {}
      if (!res.headersSent) res.status(500).end();
    });
    stream.pipe(res);
  });
});

// POST /search_by_file
// Accepts multipart/form-data with a single file field named 'file' (WAV)
// Uses a CLI `extract_index_cli <inWav> <outIndex>` if available to produce raw index bytes.
// Returns JSON: { indexBase64: string, metadata: { genre, artist, album, track, cover }, wavBase64: string }
import multer from 'multer';
const upload = multer({ dest: path.join(tmpdir(), 'sotb_uploads') });

app.post('/search_by_file', upload.single('file'), async (req, res) => {
  if (!req.file || !req.file.path) {
    return res.status(400).json({ error: 'Expected multipart/form-data with a file field named "file"' });
  }

  const uploadedPath = req.file.path;
  // Basic size check
  try {
    const st = statSync(uploadedPath);
    if (st.size === 0) {
      try { unlinkSync(uploadedPath); } catch (_) {}
      return res.status(400).json({ error: 'Uploaded file is empty' });
    }
    if (st.size > MAX_WAV_BYTES) {
      try { unlinkSync(uploadedPath); } catch (_) {}
      return res.status(413).json({ error: 'Uploaded WAV too large', maxBytes: MAX_WAV_BYTES });
    }
  } catch (err) {
    try { unlinkSync(uploadedPath); } catch (_) {}
    return res.status(500).json({ error: 'Failed to stat uploaded file', message: String(err && err.message ? err.message : err) });
  }

  // Find extract_index_cli in the same candidate locations as reconstruct_cli
  const repoRoot = path.resolve(__dirname, '..', '..', '..');
  const extractCandidates = [
    path.join(repoRoot, 'build', 'Debug', 'extract_index_cli.exe'),
    path.join(repoRoot, 'build', 'extract_index_cli.exe'),
    path.join(repoRoot, 'build', 'extract_index_cli'),
    path.join(repoRoot, '..', 'build', 'extract_index_cli.exe'),
    path.join(repoRoot, '..', 'build', 'extract_index_cli'),
    path.join(process.cwd(), 'build', 'extract_index_cli.exe'),
    path.join(process.cwd(), 'build', 'extract_index_cli'),
    path.join(repoRoot, 'extract_index_cli.exe'),
    path.join(repoRoot, 'extract_index_cli'),
  ];

  let cliPath = null;
  for (const c of extractCandidates) {
    try {
      if (c && existsSync(c)) {
        cliPath = c;
        break;
      }
    } catch (e) {}
  }

  if (!cliPath) {
    // CLI not found: return helpful 501 with instructions to build the helper CLI
    try { unlinkSync(uploadedPath); } catch (_) {}
    return res.status(501).json({
      error: 'extract_index_cli not found on server',
      message: 'Please build cpp/tools/extract_index_cli.cpp (uses AudioIndex) and place the binary on the server or in build/. See cpp/tools/README or project build instructions.'
    });
  }

  // Create temp output path for index
  const rnd = randomBytes(8).toString('hex');
  const outIndex = path.join(tmpdir(), `sotb_index_out_${rnd}.bin`);

  const child = spawn(cliPath, [uploadedPath, outIndex], { stdio: ['ignore', 'pipe', 'pipe'] });
  let stderr = '';
  child.stderr.on('data', (d) => { stderr += d.toString(); });

  let timedOut = false;
  const killTimer = setTimeout(() => {
    timedOut = true;
    try { child.kill('SIGKILL'); } catch (e) {}
  }, CHILD_TIMEOUT_MS);

  child.on('error', (err) => {
    clearTimeout(killTimer);
    try { unlinkSync(uploadedPath); } catch (_) {}
    try { unlinkSync(outIndex); } catch (_) {}
    return res.status(500).json({ error: 'Failed to run extract CLI', message: String(err && err.message ? err.message : err) });
  });

  child.on('close', (code, signal) => {
    clearTimeout(killTimer);
    if (timedOut) {
      try { unlinkSync(uploadedPath); } catch (_) {}
      try { unlinkSync(outIndex); } catch (_) {}
      return res.status(504).json({ error: 'Index extraction timed out', timeoutMs: CHILD_TIMEOUT_MS });
    }
    if (code !== 0) {
      try { unlinkSync(uploadedPath); } catch (_) {}
      try { unlinkSync(outIndex); } catch (_) {}
      return res.status(500).json({ error: 'extract_index_cli failed', code, message: stderr });
    }

    // Read produced index bytes
    try {
      if (!existsSync(outIndex)) throw new Error('Output index file not found');
      const idxBuf = readFileSync(outIndex);
      if (!idxBuf || idxBuf.length === 0) throw new Error('Index output empty');

      // Derive metadata deterministically from the index bytes (server's deriveMetadata expects raw payload)
      const derived = (function(payload) {
        const b = payload || Buffer.alloc(0);
        function token(offset, len) {
          let s = '';
          for (let i = 0; i < len; ++i) {
            const v = (offset + i < b.length) ? b[offset + i] : 0;
            const r = v % 36;
            s += (r < 10) ? String.fromCharCode(48 + (r)) : String.fromCharCode(97 + (r - 10));
          }
          return s;
        }
        const genre = token(0, 6);
        const artist = token(6, 8);
        const album = token(14, 8);
        const track = token(22, 6);
        let color = 0;
        for (let i = 0; i < 3; ++i) color = (color << 8) | (i < b.length ? b[i] : 0);
        const hex = ((color >>> 0) & 0xFFFFFF).toString(16).padStart(6, '0');
        const svg = `<svg xmlns='http://www.w3.org/2000/svg' width='256' height='256'><rect width='100%' height='100%' fill='#${hex}'/><text x='50%' y='50%' font-size='20' text-anchor='middle' fill='#fff' dominant-baseline='middle'>${track}</text></svg>`;
        const coverBase64 = Buffer.from(svg, 'utf8').toString('base64');
        return { genre, artist, album, track, coverBase64 };
      })(idxBuf);

      // Also include the uploaded WAV back to client as base64 to allow playback
      const wavBuf = readFileSync(uploadedPath);
      const wavB64 = wavBuf.toString('base64');
  const meta = { genre: derived.genre, artist: derived.artist, album: derived.album, track: derived.track, cover: `data:image/png;base64,${derived.coverBase64}` };
      const indexB64 = idxBuf.toString('base64');

      try { unlinkSync(uploadedPath); } catch (_) {}
      try { unlinkSync(outIndex); } catch (_) {}

      return res.json({ indexBase64: indexB64, metadata: meta, wavBase64: wavB64 });
    } catch (err) {
      try { unlinkSync(uploadedPath); } catch (_) {}
      try { unlinkSync(outIndex); } catch (_) {}
      return res.status(500).json({ error: 'Failed to read index output', message: String(err && err.message ? err.message : err) });
    }
  });
});

const port = process.env.PORT || 3000;
app.listen(port, () => {
  console.log(`Speaker-of-Babel server listening on port ${port}`);
});
