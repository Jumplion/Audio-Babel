import express, { json } from 'express';
import cors from 'cors';
import morgan from 'morgan';

const app = express();
app.use(cors());
app.use(morgan('dev'));
app.use(json({ limit: '50mb' }));

function normalizeBase64Url(s) {
    // Convert base64url to standard base64 and add padding
    s = s.replace(/-/g, '+').replace(/_/g, '/');
    const pad = s.length % 4;
    if (pad === 2) s += '==';
    else if (pad === 3) s += '=';
    else if (pad !== 0) throw new Error('Invalid base64/base64url string length');
    return s;
}

app.get('/health', (req, res) => { res.json({ status: 'ok' }); });

// POST /reconstruct
// Body (application/json): { format: 'base64url', data: string }
app.post('/reconstruct', (req, res) => {
    const { format, data } = req.body || {};
    if (!format || typeof data !== 'string') {
        return res.status(400).json({ error: 'Expected JSON { format: string, data: string }' });
    }

    try {
        let buf = Buffer.from(normalizeBase64Url(data), 'base64');
        res.setHeader('Content-Type', 'application/octet-stream');
        res.setHeader('Content-Disposition', 'attachment; filename="index.bin"');
        res.setHeader('X-Source-Format', format);
        res.setHeader('X-Byte-Count', String(buf.length));
        return res.send(buf);
    } catch (err) {
        console.error('decode error', err && err.stack ? err.stack : err);
        return res.status(400).json({ error: 'Failed to decode input', message: String(err && err.message ? err.message : err) });
    }
});

const port = process.env.PORT || 3000;
app.listen(port, () => { console.log(`Speaker-of-Babel server listening on port ${port}`); });
