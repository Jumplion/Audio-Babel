# WebAssembly Implementation Guide - Complete

## Overview

This guide provides step-by-step instructions for compiling the C++ Audio Index library to WebAssembly and using it in a serverless website hosted on GitHub Pages.

## What We're Building

```
Audio Index C++ Library
         ↓
   Compile to WASM
         ↓
   Load in Browser
         ↓
Generate/Decode Indexes Client-Side
```

**Result**: Fully serverless website that can generate and decode audio indexes using near-native C++ performance in the browser!

---

## Part 1: Setup and Installation

### 1.1 Install Emscripten

**Windows (PowerShell as Administrator):**
```powershell
# Clone Emscripten SDK
cd C:\
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Install latest version
.\emsdk install latest
.\emsdk activate latest

# Set environment variables (run in every new terminal)
.\emsdk_env.ps1

# Verify installation
emcc --version
# Should output: emcc (Emscripten gcc/clang-like replacement) X.X.X
```

**Linux/macOS:**
```bash
# Clone Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Install and activate
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Verify
emcc --version
```

### 1.2 Install Boost Headers for Emscripten

The Audio Index library uses `boost::multiprecision::cpp_int`. Since Boost is header-only for this feature, we just need the headers:

```bash
# Navigate to Emscripten's system include directory
cd emsdk/upstream/emscripten/system/include

# Download Boost (or use your existing Boost installation)
wget https://boostorg.jfrog.io/artifactory/main/release/1.83.0/source/boost_1_83_0.tar.gz
tar -xzf boost_1_83_0.tar.gz
mv boost_1_83_0/boost ./
rm -rf boost_1_83_0 boost_1_83_0.tar.gz
```

**Windows Alternative:**
1. Download Boost from https://www.boost.org/users/download/
2. Extract to `emsdk\upstream\emscripten\system\include\boost`

---

## Part 2: Build the WASM Module

### 2.1 Using the Build Scripts

**Windows (PowerShell):**
```powershell
# Navigate to WASM directory
cd "F:\Repos\Speaker of Babel\cpp\wasm"

# Activate Emscripten (if not already done)
C:\emsdk\emsdk_env.ps1

# Build (Release mode)
.\build-wasm.ps1

# Or build in Debug mode
.\build-wasm.ps1 -Debug

# Clean build
.\build-wasm.ps1 -Clean
```

**Linux/macOS:**
```bash
# Navigate to WASM directory
cd cpp/wasm

# Activate Emscripten (if not already done)
source /path/to/emsdk/emsdk_env.sh

# Build
chmod +x build-wasm.sh
./build-wasm.sh

# Debug mode
./build-wasm.sh --debug

# Clean build
./build-wasm.sh --clean
```

### 2.2 Manual Build (if scripts fail)

```bash
cd cpp/wasm
mkdir -p build
cd build

# Configure with Emscripten
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
emmake make -j4

# Copy files
cp audio-index.wasm ../../audio-babel-record-store/public/wasm/
cp audio-index.js ../../audio-babel-record-store/public/wasm/
```

### 2.3 Verify Build

After building, you should have:
```
audio-babel-record-store/
└── public/
    └── wasm/
        ├── audio-index.wasm  (~200-500 KB)
        └── audio-index.js    (~50-100 KB)
```

---

## Part 3: Using WASM in Your Website

### 3.1 Basic HTML Setup

```html
<!DOCTYPE html>
<html>
<head>
    <title>Audio Babel - Library Explorer</title>
</head>
<body>
    <h1>Library of Babel - Audio Index Explorer</h1>
    
    <div id="controls">
        <button id="randomBtn">Generate Random Index</button>
        <button id="exploreBtn">Explore Index</button>
    </div>
    
    <div id="output"></div>
    
    <!-- Load WASM wrapper as module -->
    <script type="module">
        import AudioIndexWASM from './js/audioIndexWasm.js';
        
        // Initialize
        const audioIndex = new AudioIndexWASM();
        await audioIndex.initialize();
        
        // Use in event handlers
        document.getElementById('randomBtn').addEventListener('click', async () => {
            const index = audioIndex.generateRandomIndex(100);
            console.log('Generated index:', index);
            
            const metadata = audioIndex.getMetadata(index);
            console.log('Metadata:', metadata);
            
            document.getElementById('output').innerHTML = `
                <h2>Generated Index</h2>
                <p><strong>Index:</strong> ${index}</p>
                <p><strong>Genre:</strong> ${metadata.genre}</p>
                <p><strong>Artist:</strong> ${metadata.artist}</p>
                <p><strong>Album:</strong> ${metadata.album}</p>
                <p><strong>Track:</strong> ${metadata.track}</p>
                <div>${metadata.cover}</div>
            `;
        });
    </script>
</body>
</html>
```

### 3.2 Complete Example: Browse and Play

```javascript
import AudioIndexWASM from './js/audioIndexWasm.js';
import { getPositionFromIndex } from './services/positionEncoder.js';

class LibraryExplorer {
    constructor() {
        this.wasm = new AudioIndexWASM();
        this.currentAudio = null;
    }
    
    async initialize() {
        console.log('Initializing Library Explorer...');
        await this.wasm.initialize();
        console.log('Ready!');
    }
    
    /**
     * Generate a random track from the library
     */
    async exploreRandom() {
        // Generate random index
        const index = this.wasm.generateRandomIndex(100);
        
        // Get position and metadata
        const position = getPositionFromIndex(index);
        const metadata = this.wasm.getMetadata(index);
        
        return {
            index,
            position,
            metadata
        };
    }
    
    /**
     * Reconstruct and play audio from index
     */
    async playIndex(base64Index) {
        try {
            // Stop current audio if playing
            if (this.currentAudio) {
                this.currentAudio.pause();
            }
            
            // Reconstruct audio samples
            console.log('Reconstructing audio...');
            const samples = this.wasm.reconstructAudio(base64Index);
            
            // Create audio element
            this.currentAudio = this.wasm.createAudioElement(samples);
            
            // Play
            await this.currentAudio.play();
            console.log('Playing audio');
            
            return this.currentAudio;
        } catch (error) {
            console.error('Error playing audio:', error);
            throw error;
        }
    }
    
    /**
     * Browse to a specific position
     */
    async browseToPosition(roomId, wall, shelf, track) {
        // In a real implementation, you'd look up the index
        // from a database or generate it
        
        // For now, generate a sample index
        const index = this.wasm.generateRandomIndex(100);
        const metadata = this.wasm.getMetadata(index);
        
        return {
            index,
            position: { roomId, wall, shelf, track },
            metadata
        };
    }
    
    /**
     * Download reconstructed audio as WAV file
     */
    async downloadAudio(base64Index, filename = 'audio.wav') {
        const samples = this.wasm.reconstructAudio(base64Index);
        const wavBlob = this.wasm.samplesToWav(samples);
        
        // Create download link
        const url = URL.createObjectURL(wavBlob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
    }
}

// Usage
const explorer = new LibraryExplorer();
await explorer.initialize();

// Explore random track
const track = await explorer.exploreRandom();
console.log('Found track:', track);

// Play it
await explorer.playIndex(track.index);

// Download it
await explorer.downloadAudio(track.index, 'track.wav');
```

### 3.3 Integration with Position Encoder

```javascript
import AudioIndexWASM from './js/audioIndexWasm.js';
import { parseIndex } from './services/indexParser.js';
import Track from './models/Track.js';

async function createTrackFromIndex(base64Index) {
    // Initialize WASM
    const wasm = new AudioIndexWASM();
    await wasm.initialize();
    
    // Parse index to get position and metadata
    const parsed = parseIndex(base64Index);
    
    // Get SVG cover from WASM
    const wasmMetadata = wasm.getMetadata(base64Index);
    
    // Reconstruct audio to get duration
    const samples = wasm.reconstructAudio(base64Index);
    const duration = samples.length / (44100 * 2); // Estimate duration
    
    // Create track object
    const track = new Track(
        parsed.index,
        parsed.position,
        parsed.metadata,
        duration,
        wasmMetadata.cover
    );
    
    return track;
}

// Usage
const track = await createTrackFromIndex('dGVzdC1hdWRpby1pbmRleA');
console.log(track.address); // "room_abc12345:W1:S05:T17"
console.log(track.getFullPath());
```

---

## Part 4: Performance Optimization

### 4.1 Lazy Loading

Only load WASM when needed:

```javascript
let wasmModule = null;

async function getWASM() {
    if (!wasmModule) {
        wasmModule = new AudioIndexWASM();
        await wasmModule.initialize();
    }
    return wasmModule;
}

// Use only when user clicks "Play" or "Explore"
document.getElementById('playBtn').addEventListener('click', async () => {
    const wasm = await getWASM();
    // ... use wasm
});
```

### 4.2 Web Workers

For heavy processing, use Web Workers:

```javascript
// worker.js
importScripts('./wasm/audio-index.js');

let wasmModule = null;

self.onmessage = async (e) => {
    const { type, data } = e.data;
    
    if (type === 'init') {
        wasmModule = await createAudioIndexModule();
        self.postMessage({ type: 'ready' });
    }
    
    if (type === 'reconstruct') {
        const samples = wasmModule._reconstructAudioFromBase64(data.index);
        self.postMessage({ type: 'result', samples });
    }
};

// Main thread
const worker = new Worker('worker.js');
worker.postMessage({ type: 'init' });
worker.onmessage = (e) => {
    if (e.data.type === 'ready') {
        console.log('Worker ready');
    }
};
```

### 4.3 Caching

Cache reconstructed audio:

```javascript
class CachedAudioIndex extends AudioIndexWASM {
    constructor() {
        super();
        this.cache = new Map();
    }
    
    reconstructAudio(base64Index) {
        if (this.cache.has(base64Index)) {
            return this.cache.get(base64Index);
        }
        
        const samples = super.reconstructAudio(base64Index);
        this.cache.set(base64Index, samples);
        return samples;
    }
}
```

---

## Part 5: Deployment to GitHub Pages

### 5.1 Directory Structure

```
audio-babel-record-store/
├── public/
│   ├── index.html
│   ├── browse.html
│   ├── track.html
│   ├── js/
│   │   ├── audioIndexWasm.js
│   │   └── app.js
│   ├── wasm/
│   │   ├── audio-index.wasm
│   │   └── audio-index.js
│   └── css/
└── package.json
```

### 5.2 MIME Types

GitHub Pages automatically serves:
- `.wasm` files as `application/wasm`
- `.js` files as `application/javascript`

No configuration needed!

### 5.3 Deploy

```bash
# Commit WASM files
git add public/wasm/
git commit -m "Add WASM module"

# Push to GitHub
git push origin main

# Enable GitHub Pages in repo settings
# Settings → Pages → Source: main branch, /audio-babel-record-store/public folder
```

Or use `gh-pages` branch:

```bash
npx gh-pages -d public
```

---

## Part 6: Troubleshooting

### Issue: "WASM module not found"

**Solution**: Ensure paths are correct:
```javascript
// If public/ is your root
import AudioIndexWASM from '/js/audioIndexWasm.js';

// The wrapper will load from '/wasm/audio-index.js'
```

### Issue: "Cannot find module"

**Solution**: Add type="module" to script tag:
```html
<script type="module" src="/js/app.js"></script>
```

### Issue: "Memory access out of bounds"

**Solution**: Increase WASM memory:
```cmake
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s INITIAL_MEMORY=33554432")
```

### Issue: Slow performance

**Solutions**:
1. Build with `-O3` optimization (already in CMakeLists)
2. Use Web Workers for heavy operations
3. Implement caching
4. Lazy-load WASM module

---

## Part 7: Testing

### 7.1 Local Testing

```bash
# Simple HTTP server
npx http-server public -p 8080

# Or Python
python3 -m http.server 8080 --directory public

# Visit http://localhost:8080
```

### 7.2 Test Script

```javascript
// test-wasm.html
import AudioIndexWASM from './js/audioIndexWASM.js';

async function runTests() {
    console.log('Starting WASM tests...');
    
    const wasm = new AudioIndexWASM();
    await wasm.initialize();
    console.log('✓ WASM initialized');
    
    // Test 1: Generate random index
    const index = wasm.generateRandomIndex(50);
    console.log('✓ Generated index:', index);
    
    // Test 2: Validate index
    const isValid = wasm.validateIndex(index);
    console.assert(isValid, 'Index should be valid');
    console.log('✓ Index validated');
    
    // Test 3: Get metadata
    const metadata = wasm.getMetadata(index);
    console.assert(metadata.genre, 'Should have genre');
    console.log('✓ Metadata extracted:', metadata);
    
    // Test 4: Calculate size
    const size = wasm.calculateAudioSize(120);
    console.assert(size > 0, 'Size should be positive');
    console.log('✓ Calculated size:', size);
    
    console.log('All tests passed!');
}

runTests().catch(console.error);
```

---

## Summary

You now have a **fully serverless** Audio Babel Record Store that:

✅ Runs entirely in the browser  
✅ Uses C++ performance via WebAssembly  
✅ Hosts for free on GitHub Pages  
✅ Generates/decodes indexes client-side  
✅ No server or backend needed  

**Next steps**:
1. Build the WASM module
2. Create UI for exploring the library
3. Add audio playback controls
4. Implement search and navigation
5. Deploy to GitHub Pages

The complete implementation is ready to use!
