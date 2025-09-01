import express, { json } from 'express';
import cors from 'cors';
import morgan from 'morgan';
import { fileURLToPath } from 'url';
// POST /reconstruct
// Body (application/json): { format: 'base64', data: string }
import { tmpdir } from 'os';
import { randomBytes } from 'crypto';
import { writeFileSync, unlinkSync, createReadStream, existsSync, statSync } from 'fs';
import { spawn } from 'child_process';
import path from 'path';

const app = express();
app.use(cors());
app.use(morgan('dev'));
app.use(json({ limit: '50mb' }));

// Resolve __dirname for ES modules and serve static frontend (public)
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const frontendPublic = path.resolve(path.join(__dirname, '..', '..', 'frontend', 'public'));
try { require('fs').accessSync(frontendPublic); console.log('Serving static frontend from', frontendPublic); } catch(e) { console.log('Frontend public not found at', frontendPublic); }
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

app.get('/health', (req, res) => { res.json({ status: 'ok' }); });

// Limits (tunable)
const MAX_INDEX_BYTES = 5 * 1024 * 1024; // 5 MB max raw index input
const MAX_WAV_BYTES = 50 * 1024 * 1024;  // 50 MB max reconstructed WAV
const CHILD_TIMEOUT_MS = 20 * 1000;      // 20s timeout for reconstruction

app.post('/reconstruct', async (req, res) => {
    const { format, data } = req.body || {};
    if (!format || typeof data !== 'string') {
        return res.status(400).json({ error: 'Expected JSON { format: string, data: string }' });
    }

    if (!['base64', 'base64url'].includes(format.toLowerCase())) {
        return res.status(400).json({ error: 'This endpoint accepts only base64 or base64url formats for direct reconstruction' });
    }

    let buf;
    try {
        buf = Buffer.from(normalizeBase64(data), 'base64');
    } catch (err) {
        return res.status(400).json({ error: 'Failed to decode base64 input', message: String(err && err.message ? err.message : err) });
    }

    // Basic validation: non-empty and size limit
    if (!buf || buf.length === 0) {
        return res.status(400).json({ error: 'Decoded input is empty' });
    }
    if (buf.length > MAX_INDEX_BYTES) {
        return res.status(413).json({ error: 'Input too large', maxBytes: MAX_INDEX_BYTES });
    }

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

        console.log(`Decoded header -> sampleRate=${sampleRate} bitDepth=${bitDepth} numChannels=${numChannels} numFrames=${numFrames}`);

        const ALLOWED_BIT_DEPTHS = new Set([8, 16, 24, 32]);
        if (!ALLOWED_BIT_DEPTHS.has(bitDepth)) {
            return res.status(422).json({ error: 'Unsupported bit depth in index header', bitDepth, allowed: Array.from(ALLOWED_BIT_DEPTHS) });
        }
    } catch (err) {
        console.error('Header parse error', err);
        return res.status(400).json({ error: 'Failed to parse header from decoded input', message: String(err && err.message ? err.message : err) });
    }

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
        path.join(repoRoot, 'reconstruct_cli')
    ];

    let cliPath = null;
    for (const c of candidates) {
        try { if (c && existsSync(c)) { cliPath = c; break; } } catch(e){}
    }
    if (cliPath) console.log('Using reconstruct_cli at:', cliPath);
    // fallback to assuming it's on PATH
    if (!cliPath) cliPath = 'reconstruct_cli';
    const child = spawn(cliPath, [inPath, outPath], { stdio: ['ignore', 'pipe', 'pipe'] });
    let stderr = '';
    child.stderr.on('data', (d) => { stderr += d.toString(); });

    let timedOut = false;
    const killTimer = setTimeout(() => {
        timedOut = true;
        try { child.kill('SIGKILL'); } catch (e) {}
    }, CHILD_TIMEOUT_MS);

    child.on('error', (err) => {
        clearTimeout(killTimer);
        console.error('Failed to spawn reconstruct_cli', err);
        try { unlinkSync(inPath); } catch(_){}
        return res.status(500).json({ error: 'Failed to run recon CLI', message: String(err && err.message ? err.message : err) });
    });

    // Handle child process close
    child.on('close', (code, signal) => {
        clearTimeout(killTimer);
        if (timedOut) {
            console.error('reconstruct_cli timed out', CHILD_TIMEOUT_MS);
            try { unlinkSync(inPath); } catch(_){}
            try { unlinkSync(outPath); } catch(_){}
            return res.status(504).json({ error: 'Reconstruction timed out', timeoutMs: CHILD_TIMEOUT_MS });
        }

        if (code !== 0) {
            console.error('reconstruct_cli failed', code, stderr);
            try { unlinkSync(inPath); } catch(_){}
            try { unlinkSync(outPath); } catch(_){}
            return res.status(500).json({ error: 'Reconstruction failed', code, message: stderr });
        }

        // Inspect output size before streaming
        try {
            if (!existsSync(outPath)) {
                throw new Error('Output file not found');
            }
            const st = statSync(outPath);
            if (st.size > MAX_WAV_BYTES) {
                try { unlinkSync(inPath); } catch(_){}
                try { unlinkSync(outPath); } catch(_){}
                return res.status(413).json({ error: 'Reconstructed WAV too large', maxBytes: MAX_WAV_BYTES });
            }
        } catch (err) {
            console.error('Failed to stat output file', err);
            try { unlinkSync(inPath); } catch(_){}
            try { unlinkSync(outPath); } catch(_){}
            return res.status(500).json({ error: 'Failed to read reconstructed output', message: String(err && err.message ? err.message : err) });
        }

        // Stream the WAV back
        res.setHeader('Content-Type', 'audio/wav');
        res.setHeader('Content-Disposition', 'attachment; filename="reconstructed.wav"');
        const stream = createReadStream(outPath);
        stream.on('end', () => {
            try { unlinkSync(inPath); } catch(_){}
            try { unlinkSync(outPath); } catch(_){}
        });
        stream.on('error', (err) => {
            console.error('Stream error', err);
            try { unlinkSync(inPath); } catch(_){}
            try { unlinkSync(outPath); } catch(_){}
            if (!res.headersSent) res.status(500).end();
        });
        stream.pipe(res);
    });
});

const port = process.env.PORT || 3000;
app.listen(port, () => { console.log(`Speaker-of-Babel server listening on port ${port}`); });
