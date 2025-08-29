# Speaker of Babel - minimal server

This small Node/Express server accepts encoded index strings (various formats) and returns the decoded raw bytes as an octet-stream. The intent is to let the C++ tools (or another worker) consume the binary and convert it to WAV.

Quick start (Windows PowerShell):

```powershell
cd "web/server"
npm install
npm start
```

API

POST /reconstruct

- Content-Type: application/json
- Body: { "format": "base64url", "data": "..." }

Returns: application/octet-stream (attachment `index.bin`) containing the decoded bytes.

Examples

Base64URL (from browser-safe encoding):

```powershell
$body = @{ format = 'base64url'; data = 'Abc-_' } | ConvertTo-Json
curl -X POST http://localhost:3000/reconstruct -H "Content-Type: application/json" -d $body --output index.bin
```

Notes / Next steps

- Currently the server only decodes input and returns raw bytes. To convert the bytes to a WAV file, you can invoke the C++ binary (e.g. a small CLI helper that calls `indexToAudioData` and `writeAudioDataToFile`) and return the resulting WAV to the client; I can implement that integration next if you want.
