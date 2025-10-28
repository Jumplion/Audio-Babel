// browse.js - Hierarchical navigation through the Record Shop library
import AudioIndexWASM from './audioIndexWasm.js';
import { calculateDuration } from './audioIndex.js';
import { bytesToBase64Chunked } from './utils.js';

// Library hierarchy constants (from C++)
const TRACKS_PER_ALBUM = 15;
const ALBUMS_PER_SHELF = 32;
const SHELVES_PER_WALL = 5;
const WALLS_PER_ROOM = 4;

// Initialize WASM module (lazy-loaded)
let wasmModule = null;
async function getWasmModule() {
    if (!wasmModule) {
        wasmModule = new AudioIndexWASM();
        await wasmModule.initialize();
    }
    return wasmModule;
}

// Current navigation state
const navState = {
    room: null,
    wall: null,
    shelf: null,
    album: null,
    track: null
};

function escapeHtml(s) {
    if (!s && s !== 0) return '';
    return String(s)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

function $(id) {
    return document.getElementById(id);
}

/**
 * Update the breadcrumb navigation
 */
function updateBreadcrumb() {
    const nav = $('breadcrumb');
    if (!nav) return;
    
    nav.innerHTML = '';
    
    const parts = [];
    
    if (navState.room !== null) {
        const link = document.createElement('a');
        link.href = '#';
        link.textContent = `Room ${navState.room}`;
        link.addEventListener('click', (e) => {
            e.preventDefault();
            goToRoom();
        });
        parts.push(link);
    }
    
    if (navState.wall !== null) {
        const link = document.createElement('a');
        link.href = '#';
        link.textContent = `Wall ${navState.wall}`;
        link.addEventListener('click', (e) => {
            e.preventDefault();
            goToWall();
        });
        parts.push(link);
    }
    
    if (navState.shelf !== null) {
        const link = document.createElement('a');
        link.href = '#';
        link.textContent = `Shelf ${navState.shelf}`;
        link.addEventListener('click', (e) => {
            e.preventDefault();
            goToShelf();
        });
        parts.push(link);
    }
    
    if (navState.album !== null) {
        const link = document.createElement('a');
        link.href = '#';
        link.textContent = `Album ${navState.album}`;
        link.addEventListener('click', (e) => {
            e.preventDefault();
            goToAlbum();
        });
        parts.push(link);
    }
    
    if (navState.track !== null) {
        const span = document.createElement('span');
        span.textContent = `Track ${navState.track}`;
        span.style.fontWeight = '600';
        parts.push(span);
    }
    
    parts.forEach((part, i) => {
        nav.appendChild(part);
        if (i < parts.length - 1) {
            const sep = document.createElement('span');
            sep.textContent = ' › ';
            sep.style.margin = '0 8px';
            sep.style.color = 'var(--muted)';
            nav.appendChild(sep);
        }
    });
}

/**
 * Show only the specified section
 */
function showSection(sectionId) {
    const sections = ['roomSection', 'wallSection', 'shelfSection', 'albumSection', 'trackSection', 'resultSection'];
    sections.forEach(id => {
        const el = $(id);
        if (el) el.style.display = (id === sectionId) ? 'block' : 'none';
    });
}

/**
 * Navigate to room selection
 */
function goToRoom() {
    navState.wall = null;
    navState.shelf = null;
    navState.album = null;
    navState.track = null;
    
    showSection('roomSection');
    showSection('wallSection');
    updateBreadcrumb();
}

/**
 * Navigate to wall selection
 */
function goToWall() {
    navState.shelf = null;
    navState.album = null;
    navState.track = null;
    
    showSection('wallSection');
    updateBreadcrumb();
}

/**
 * Navigate to shelf selection
 */
function goToShelf() {
    navState.album = null;
    navState.track = null;
    
    showSection('shelfSection');
    renderShelves();
    updateBreadcrumb();
}

/**
 * Navigate to album selection
 */
function goToAlbum() {
    navState.track = null;
    
    showSection('albumSection');
    renderAlbums();
    updateBreadcrumb();
}

/**
 * Enter a room
 */
function enterRoom() {
    const input = $('roomInput');
    if (!input) return;
    
    const roomNum = input.value.trim();
    if (roomNum === '') return;
    
    try {
        const room = BigInt(roomNum);
        if (room < 0n) {
            alert('Room number must be 0 or greater');
            return;
        }
        
        navState.room = room;
        navState.wall = null;
        navState.shelf = null;
        navState.album = null;
        navState.track = null;
        
        showSection('wallSection');
        updateBreadcrumb();
    } catch (e) {
        alert('Invalid room number');
    }
}

/**
 * Select a wall
 */
function selectWall(wallNum) {
    if (navState.room === null) return;
    
    navState.wall = wallNum;
    navState.shelf = null;
    navState.album = null;
    navState.track = null;
    
    showSection('shelfSection');
    renderShelves();
    updateBreadcrumb();
}

/**
 * Render the shelves for the current wall
 */
function renderShelves() {
    const container = $('shelvesContainer');
    if (!container) return;
    
    container.innerHTML = '';
    
    for (let i = 0; i < SHELVES_PER_WALL; i++) {
        const btn = document.createElement('button');
        btn.className = 'shelf-btn';
        btn.textContent = `Shelf ${i}`;
        btn.addEventListener('click', () => selectShelf(i));
        container.appendChild(btn);
    }
}

/**
 * Select a shelf
 */
function selectShelf(shelfNum) {
    if (navState.room === null || navState.wall === null) return;
    
    navState.shelf = shelfNum;
    navState.album = null;
    navState.track = null;
    
    showSection('albumSection');
    renderAlbums();
    updateBreadcrumb();
}

/**
 * Render the albums for the current shelf
 */
function renderAlbums() {
    const container = $('albumsContainer');
    if (!container) return;
    
    container.innerHTML = '';
    
    for (let i = 0; i < ALBUMS_PER_SHELF; i++) {
        const btn = document.createElement('button');
        btn.className = 'album-btn';
        btn.textContent = `Album ${i}`;
        btn.addEventListener('click', () => selectAlbum(i));
        container.appendChild(btn);
    }
}

/**
 * Select an album
 */
function selectAlbum(albumNum) {
    if (navState.room === null || navState.wall === null || navState.shelf === null) return;
    
    navState.album = albumNum;
    navState.track = null;
    
    showSection('trackSection');
    renderTracks();
    updateBreadcrumb();
}

/**
 * Render the tracks for the current album
 */
function renderTracks() {
    const container = $('tracksContainer');
    if (!container) return;
    
    container.innerHTML = '';
    
    for (let i = 0; i < TRACKS_PER_ALBUM; i++) {
        const btn = document.createElement('button');
        btn.className = 'track-btn';
        btn.textContent = `Track ${i}`;
        btn.addEventListener('click', () => selectTrack(i));
        container.appendChild(btn);
    }
}

/**
 * Select a track and display the result
 */
async function selectTrack(trackNum) {
    if (navState.room === null || navState.wall === null || 
        navState.shelf === null || navState.album === null) return;
    
    navState.track = trackNum;
    
    showSection('resultSection');
    updateBreadcrumb();
    
    await generateAndDisplayTrack();
}

/**
 * Generate and display the selected track
 */
async function generateAndDisplayTrack() {
    const container = $('resultContainer');
    if (!container) return;
    
    container.innerHTML = '<p>Generating track...</p>';
    
    try {
        // Get WASM module
        const wasm = await getWasmModule();
        
        // Reconstruct base64 index from position using WASM
        const base64Index = wasm.module.reconstructIndex(
            navState.room.toString(),
            navState.wall,
            navState.shelf,
            navState.album,
            navState.track
        );
        
        if (!base64Index || base64Index.startsWith('error:')) {
            throw new Error(base64Index || 'Failed to reconstruct index');
        }
        
        // Get metadata from WASM
        const metadataJson = wasm.module.getMetadata(base64Index);
        const metadata = JSON.parse(metadataJson);
        
        if (metadata.error) {
            throw new Error(metadata.error);
        }
        
        // Decode audio from index
        const pcmData = wasm.reconstructAudioFromIndex(base64Index);
        
        // Calculate duration
        const duration = calculateDuration(pcmData.length, 44100, 16, 1);
        
        // Generate WAV blob
        const wavBlob = wasm.samplesToWav(pcmData, 44100, 16, 1);
        const wavArrayBuffer = await wavBlob.arrayBuffer();
        const wavBytes = new Uint8Array(wavArrayBuffer);
        const wavBase64 = bytesToBase64Chunked(wavBytes);
        
        // Display results
        container.innerHTML = '';
        
        // Position info
        const posInfo = document.createElement('div');
        posInfo.style.marginBottom = '16px';
        posInfo.style.padding = '12px';
        posInfo.style.background = 'rgba(30, 36, 51, 0.4)';
        posInfo.style.borderRadius = '8px';
        posInfo.innerHTML = `
            <div style="font-size:13px; color:var(--muted); margin-bottom:8px">
                <strong>Position:</strong> Room ${escapeHtml(navState.room)}, Wall ${navState.wall}, Shelf ${navState.shelf}, Album ${navState.album}, Track ${navState.track}
            </div>
            <div style="font-size:13px; color:var(--muted)">
                <strong>Index:</strong> ${escapeHtml(base64Index.substring(0, 50))}${base64Index.length > 50 ? '...' : ''}
            </div>
        `;
        container.appendChild(posInfo);
        
        // Cover and metadata
        const metaContainer = document.createElement('div');
        metaContainer.style.display = 'flex';
        metaContainer.style.gap = '16px';
        metaContainer.style.marginBottom = '16px';
        metaContainer.style.alignItems = 'flex-start';
        
        // Cover
        if (metadata.cover) {
            const coverDiv = document.createElement('div');
            coverDiv.innerHTML = metadata.cover;
            coverDiv.style.flexShrink = '0';
            const svg = coverDiv.querySelector('svg');
            if (svg) {
                svg.style.width = '128px';
                svg.style.height = '128px';
                svg.style.borderRadius = '6px';
            }
            metaContainer.appendChild(coverDiv);
        }
        
        // Metadata
        const metaDiv = document.createElement('div');
        metaDiv.innerHTML = `
            <div style="font-weight:700; font-size:18px; margin-bottom:6px">${escapeHtml(metadata.track)}</div>
            <div style="color:var(--muted); margin-bottom:4px">${escapeHtml(metadata.artist)}</div>
            <div style="color:var(--muted); font-size:14px; margin-bottom:4px">${escapeHtml(metadata.album)}</div>
            <div style="color:var(--muted); font-size:13px">Genre: ${escapeHtml(metadata.genre)}</div>
            <div style="color:var(--muted); font-size:13px; margin-top:6px">Duration: ${duration.toFixed(2)}s</div>
        `;
        metaContainer.appendChild(metaDiv);
        
        container.appendChild(metaContainer);
        
        // Audio player
        const byteChars = atob(wavBase64);
        const len = byteChars.length;
        const bytes = new Uint8Array(len);
        for (let i = 0; i < len; ++i) bytes[i] = byteChars.charCodeAt(i);
        const blob = new Blob([bytes], { type: 'audio/wav' });
        const url = URL.createObjectURL(blob);
        
        const audio = document.createElement('audio');
        audio.controls = true;
        audio.src = url;
        audio.style.display = 'block';
        audio.style.marginBottom = '12px';
        audio.style.width = '100%';
        container.appendChild(audio);
        
        // Download link
        const downloadBtn = document.createElement('a');
        downloadBtn.href = url;
        downloadBtn.download = `track_${navState.room}_${navState.wall}_${navState.shelf}_${navState.album}_${navState.track}.wav`;
        downloadBtn.textContent = 'Download .wav';
        downloadBtn.className = 'btn';
        downloadBtn.style.display = 'inline-block';
        container.appendChild(downloadBtn);
        
    } catch (err) {
        console.error('Error generating track:', err);
        container.innerHTML = `<p style="color:var(--error)">Error: ${escapeHtml(err.message || String(err))}</p>`;
    }
}

/**
 * Initialize the page
 */
function init() {
    // Room input
    const roomInput = $('roomInput');
    const enterRoomBtn = $('enterRoomBtn');
    
    if (enterRoomBtn) {
        enterRoomBtn.addEventListener('click', enterRoom);
    }
    
    if (roomInput) {
        roomInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                e.preventDefault();
                enterRoom();
            }
        });
    }
    
    // Wall selection (SVG hexagon)
    const walls = document.querySelectorAll('.wall');
    walls.forEach(wall => {
        const wallNum = parseInt(wall.dataset.wall);
        wall.addEventListener('click', () => selectWall(wallNum));
        wall.addEventListener('mouseenter', () => wall.classList.add('hover'));
        wall.addEventListener('mouseleave', () => wall.classList.remove('hover'));
    });
    
    // Initialize breadcrumb
    updateBreadcrumb();
    
    // Show room section by default
    showSection('roomSection');
}

console.info('browse.js (hierarchical) loaded');
document.addEventListener('DOMContentLoaded', () => {
    console.info('browse.js DOMContentLoaded');
    try {
        init();
    } catch (e) {
        console.error('browse.init error', e);
    }
});