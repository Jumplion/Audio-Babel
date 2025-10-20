# WebAssembly Quick Reference

## Build Commands

### Windows (PowerShell)
```powershell
# Activate Emscripten
C:\emsdk\emsdk_env.ps1

# Build
cd cpp\wasm
.\build-wasm.ps1

# Clean build
.\build-wasm.ps1 -Clean -Debug
```

### Linux/macOS
```bash
# Activate Emscripten
source ~/emsdk/emsdk_env.sh

# Build
cd cpp/wasm
./build-wasm.sh

# Clean build
./build-wasm.sh --clean --debug
```

## JavaScript API

### Initialization
```javascript
import AudioIndexWASM from './js/audioIndexWasm.js';

const wasm = new AudioIndexWASM();
await wasm.initialize();
```

### Generate Random Index
```javascript
const index = wasm.generateRandomIndex(100);
// Returns: "dGVzdC1hdWRpby1pbmRleC1leGFtcGxl..."
```

### Get Metadata
```javascript
const metadata = wasm.getMetadata(index);
// Returns: { genre, artist, album, track, cover }
```

### Reconstruct Audio
```javascript
const samples = wasm.reconstructAudio(index);
// Returns: Uint8Array of PCM samples
```

### Play Audio
```javascript
const audio = wasm.createAudioElement(samples);
await audio.play();
```

### Validate Index
```javascript
const isValid = wasm.validateIndex(index);
// Returns: true/false
```

### Calculate Size
```javascript
const bytes = wasm.calculateAudioSize(120); // 120 seconds
// Returns: ~10,584,000 bytes
```

### Download WAV
```javascript
const samples = wasm.reconstructAudio(index);
const blob = wasm.samplesToWav(samples);
// Create download link...
```

## Integration Example

```javascript
import AudioIndexWASM from './js/audioIndexWasm.js';
import { getPositionFromIndex } from './services/positionEncoder.js';

const wasm = new AudioIndexWASM();
await wasm.initialize();

// Generate and explore
const index = wasm.generateRandomIndex(100);
const position = getPositionFromIndex(index);
const metadata = wasm.getMetadata(index);

console.log(`Found at: ${position.address}`);
console.log(`Metadata: ${metadata.genre} / ${metadata.artist}`);

// Play
const samples = wasm.reconstructAudio(index);
const audio = wasm.createAudioElement(samples);
await audio.play();
```

## File Locations

After building:
```
public/
└── wasm/
    ├── audio-index.wasm  (~200-500 KB)
    └── audio-index.js    (~50-100 KB)
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Module not found | Check path: `/wasm/audio-index.js` |
| Memory error | Increase `INITIAL_MEMORY` in CMakeLists |
| Slow performance | Use Web Workers, enable caching |
| Build fails | Re-run `emsdk_env` script |

## Deploy to GitHub Pages

```bash
# Build WASM
./build-wasm.sh

# Commit
git add public/wasm/
git commit -m "Add WASM module"

# Push
git push origin main

# Enable GitHub Pages in repo settings
```

## Performance Tips

1. **Lazy load**: Only initialize WASM when needed
2. **Cache**: Store reconstructed audio
3. **Web Workers**: Offload heavy processing
4. **Preload**: Use `<link rel="preload">` for WASM

## Next Steps

1. ✅ Build WASM module
2. ✅ Test locally
3. 🔲 Create UI for library exploration
4. 🔲 Add audio player controls
5. 🔲 Implement search
6. 🔲 Deploy to GitHub Pages

---

See `WASM_IMPLEMENTATION_GUIDE.md` for complete documentation.
