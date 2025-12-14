// browse.js - Hierarchical navigation through the Record Shop library
import { getWasmModule } from '../core/wasmModule.js';
import { calculateDuration } from '../utils/audioIndex.js';
import { addIndexHeader, decodeBase64Url, escapeHtml, indexToBase64 } from '../utils/utils.js';
import { showValidationError, handleError } from '../utils/errorHandler.js';
import { handleJsonResponse, cleanupResultHandler } from '../core/resultHandler.js';

// Library hierarchy constants (from C++)
const TRACKS_PER_ALBUM = 15;
const ALBUMS_PER_SHELF = 32;
const SHELVES_PER_WALL = 5;
const WALLS_PER_ROOM = 4;

// Current navigation state
const navState = {
    room: null,
    wall: null,
    shelf: null,
    album: null,
    track: null
};

/**
 * Clamp a position value to its valid range
 * This ensures values sent to WASM are always within the correct bounds,
 * preventing arithmetic overflow in the C++ reconstructIndexFromPosition function.
 * 
 * @param {number} value - Value to clamp
 * @param {number} max - Maximum valid value (inclusive)
 * @param {string} name - Field name for logging
 * @returns {number} Clamped value (0 to max)
 */
function clampPositionValue(value, max, name) {
    const original = value;
    let clamped = Math.floor(value); // Ensure integer
    
    // Clamp to valid range [0, max]
    if (clamped < 0) {
        clamped = 0;
    } else if (clamped > max) {
        clamped = max;
    }
    
    // Log if clamping occurred (helps with debugging)
    if (clamped !== original) {
        console.warn(`Position value clamped: ${name} ${original} → ${clamped} (valid range: 0-${max})`);
    }
    
    return clamped;
}

/**
 * Validate and clamp all position values before sending to WASM
 * Ensures wall, shelf, album, and track are within their valid ranges.
 * The room string is validated separately (base64 format).
 * 
 * @param {number} wall - Wall number (will be clamped to 0-3)
 * @param {number} shelf - Shelf number (will be clamped to 0-4)
 * @param {number} album - Album number (will be clamped to 0-31)
 * @param {number} track - Track number (will be clamped to 0-14)
 * @returns {Object} Validated position with clamped values
 */
function validatePositionValues(wall, shelf, album, track) {
    return {
        wall: clampPositionValue(wall, WALLS_PER_ROOM - 1, 'wall'),
        shelf: clampPositionValue(shelf, SHELVES_PER_WALL - 1, 'shelf'),
        album: clampPositionValue(album, ALBUMS_PER_SHELF - 1, 'album'),
        track: clampPositionValue(track, TRACKS_PER_ALBUM - 1, 'track')
    };
}

/**
 * Get element by ID (shorthand helper)
 * @param {string} id - Element ID
 * @returns {HTMLElement|null} Element or null if not found
 */
function $(id) {
    return document.getElementById(id);
}

/**
 * Update the breadcrumb navigation display
 * Shows the current position in the hierarchy with clickable links
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
 * Show only the specified section, hiding all others
 * @param {string} sectionId - ID of section to display
 */
function showSection(sectionId) {
    const sections = ['roomSection', 'wallSection', 'shelfSection', 'albumSection', 'trackSection', 'resultSection'];
    sections.forEach(id => {
        const el = $(id);
        if (el) el.style.display = (id === sectionId) ? 'block' : 'none';
    });
    
    // Clean up result handler when leaving result section
    if (sectionId !== 'resultSection') {
        cleanupResultHandler();
    }
}

/**
 * Clear navigation state from a given level downward
 * @param {string[]} levelsToClear - Navigation levels to clear (e.g., ['wall', 'shelf', 'album', 'track'])
 */
function clearNavLevels(levelsToClear) {
    levelsToClear.forEach(level => {
        navState[level] = null;
    });
}

/**
 * Navigate to a specific level in the hierarchy
 * @param {string} section - Section to show (e.g., 'wallSection')
 * @param {string[]} clearLevels - Levels to clear from nav state
 * @param {Function} [renderFn] - Optional render function to call after navigation
 */
function navigateToLevel(section, clearLevels, renderFn = null) {
    clearNavLevels(clearLevels);
    showSection(section);
    if (renderFn) renderFn();
    updateBreadcrumb();
}

/**
 * Navigate to room selection
 */
function goToRoom() {
    clearNavLevels(['wall', 'shelf', 'album', 'track']);
    showSection('roomSection');
    showSection('wallSection');
    updateBreadcrumb();
}

/**
 * Navigate to wall selection
 */
function goToWall() {
    navigateToLevel('wallSection', ['shelf', 'album', 'track']);
}

/**
 * Navigate to shelf selection
 */
function goToShelf() {
    navigateToLevel('shelfSection', ['album', 'track'], renderShelves);
}

/**
 * Navigate to album selection
 */
function goToAlbum() {
    navigateToLevel('albumSection', ['track'], renderAlbums);
}

/**
 * Enter a room by ID or base64 string
 * Validates input and converts numeric room IDs to base64 format
 */
function enterRoom() {
    const input = $('roomInput');
    if (!input) return;
    
    const roomInput = input.value.trim();
    
    // Room can be empty string (for room 0) or a base64 string
    // Accept numeric input and convert to base64, or accept base64 directly
    let room;
    
    if (roomInput === '' || roomInput === '0') {
        // Empty string or "0" both represent room 0
        room = "";
    } else if (/^\d+$/.test(roomInput)) {
        // Numeric input - convert to base64
        try {
            const roomNum = BigInt(roomInput);
            if (roomNum < 0n) {
                showValidationError('Room number must be 0 or greater');
                return;
            }
            // Convert room number to base64 using our encoder
            room = indexToBase64(roomNum);
        } catch (e) {
            showValidationError('Invalid room number');
            return;
        }
    } else {
        // Assume it's already a base64 string
        room = roomInput;
    }
    
    navState.room = room;
    navState.wall = null;
    navState.shelf = null;
    navState.album = null;
    navState.track = null;
    
    showSection('wallSection');
    updateBreadcrumb();
}

/**
 * Select a wall and navigate to shelf selection
 * @param {number} wallNum - Wall number (0-3)
 */
function selectWall(wallNum) {
    if (navState.room === null) return;
    
    navState.wall = wallNum;
    clearNavLevels(['shelf', 'album', 'track']);
    
    showSection('shelfSection');
    renderShelves();
    updateBreadcrumb();
}

/**
 * Render numbered buttons in a container
 * @param {string} containerId - ID of container element
 * @param {number} count - Number of buttons to create
 * @param {string} className - CSS class for buttons
 * @param {Function} clickHandler - Click handler function
 * @param {Function} [labelFn] - Optional function to generate button label (default: index number)
 */
function renderButtons(containerId, count, className, clickHandler, labelFn = (i) => `${i}`) {
    const container = $(containerId);
    if (!container) return;
    
    container.innerHTML = '';
    
    for (let i = 0; i < count; i++) {
        const btn = document.createElement('button');
        btn.className = className;
        btn.textContent = labelFn(i);
        btn.addEventListener('click', () => clickHandler(i));
        container.appendChild(btn);
    }
}

/**
 * Render the shelves for the current wall
 * Creates numbered buttons (0-4) for shelf selection
 */
function renderShelves() {
    renderButtons('shelvesContainer', SHELVES_PER_WALL, 'shelf-btn', selectShelf);
}

/**
 * Select a shelf and navigate to album selection
 * Validates that room and wall have been selected first
 * @param {number} shelfNum - Shelf number (0-4)
 */
function selectShelf(shelfNum) {
    if (navState.room === null || navState.wall === null) return;
    
    navState.shelf = shelfNum;
    clearNavLevels(['album', 'track']);
    
    showSection('albumSection');
    renderAlbums();
    updateBreadcrumb();
}

/**
 * Render the albums for the current shelf
 * Creates numbered buttons (0-31) for album selection
 */
function renderAlbums() {
    renderButtons('albumsContainer', ALBUMS_PER_SHELF, 'album-btn', selectAlbum);
}

/**
 * Select an album and navigate to track selection
 * Validates that room, wall, and shelf have been selected first
 * @param {number} albumNum - Album number (0-31)
 */
function selectAlbum(albumNum) {
    if (navState.room === null || navState.wall === null || navState.shelf === null) return;
    
    navState.album = albumNum;
    clearNavLevels(['track']);
    
    showSection('trackSection');
    renderTracks();
    updateBreadcrumb();
}

/**
 * Render the tracks for the current album
 * Creates numbered buttons (0-14) labeled "Track N" for track selection
 */
function renderTracks() {
    renderButtons('tracksContainer', TRACKS_PER_ALBUM, 'track-btn', selectTrack, (i) => `Track ${i}`);
}

/**
 * Select a track and display its audio content
 * Validates that all parent levels have been selected
 * @param {number} trackNum - Track number (0-14)
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
 * Reconstructs the audio index from the current position,
 * extracts metadata, generates audio, and displays the result
 * with playback controls and download options
 */
async function generateAndDisplayTrack() {
    const container = $('resultContainer');
    if (!container) return;
    
    // Clean up any existing result state before generating new one
    cleanupResultHandler();
    
    // Show a more descriptive loading message
    container.innerHTML = '<p>Generating track... This may take a moment for longer audio.</p>';
    
    try {
        console.log('Starting track generation for position:', {
            room: navState.room,
            wall: navState.wall,
            shelf: navState.shelf,
            album: navState.album,
            track: navState.track
        });
        
        // Get WASM module
        const wasm = await getWasmModule();
        console.log('WASM module ready');
        
        // Validate and clamp position values before sending to WASM
        // This ensures values are within valid ranges (wall: 0-3, shelf: 0-4, album: 0-31, track: 0-14)
        const validatedPos = validatePositionValues(
            navState.wall,
            navState.shelf,
            navState.album,
            navState.track
        );
        
        // Reconstruct base64 index from position using WASM
        // This returns the position index (PCM data only, no header)
        const positionIndexBase64 = wasm.module.reconstructIndex(
            navState.room.toString(),
            validatedPos.wall,
            validatedPos.shelf,
            validatedPos.album,
            validatedPos.track
        );
        console.log('Position index (PCM only):', positionIndexBase64?.substring(0, 50) + '...');
        
        if (!positionIndexBase64 || positionIndexBase64.startsWith('error:')) {
            throw new Error(positionIndexBase64 || 'Failed to reconstruct position index');
        }
        
        // The position index is just the PCM data. We need to add a header to make it a valid audio index.
        // Decode to get actual byte count
        const pcmBytes = decodeBase64Url(positionIndexBase64);
        const bytesPerSample = 16 / 8; // 16-bit
        const numChannels = 1; // mono
        const numFrames = Math.floor(pcmBytes.length / bytesPerSample / numChannels);
        
        // Add 13-byte header to create a valid audio index
        console.log('PCM size:', pcmBytes.length, 'bytes, numFrames:', numFrames);
        const base64Index = addIndexHeader(positionIndexBase64, {
            numFrames: numFrames,
            sampleRate: 44100,
            bitDepth: 16,
            numChannels: 1
        });
        console.log('Full audio index (with header):', base64Index?.substring(0, 50) + '...');
        
        // Get metadata from WASM
        console.log('Getting metadata...');
        const metadataJson = wasm.module.getMetadata(base64Index);
        const metadata = JSON.parse(metadataJson);
        
        if (metadata.error) {
            throw new Error(metadata.error);
        }
        console.log('Metadata retrieved:', metadata);
        
        // Decode audio from index
        console.log('Reconstructing audio from index...');
        const pcmData = wasm.reconstructAudioFromIndex(base64Index);
        console.log('PCM data reconstructed, size:', pcmData?.length, 'bytes');
        
        // Generate WAV blob
        console.log('Creating WAV blob...');
        const wavBlob = wasm.samplesToWav(pcmData, 44100, 16, 1);
        const wavArrayBuffer = await wavBlob.arrayBuffer();
        const wavBytes = new Uint8Array(wavArrayBuffer);
        
        // Convert to base64 for audio player
        const wavBase64 = btoa(String.fromCharCode(...wavBytes));
        
        // Create result object for handleJsonResponse
        const result = {
            indexBase64: base64Index,
            metadata: metadata,
            wavBase64: wavBase64
        };
        
        // Use the shared result handler for consistent UI
        await handleJsonResponse(result, base64Index);
        
    } catch (err) {
        handleError('browse.js:generateAndDisplayTrack', err, err.message || String(err));
    }
}

/**
 * Initialize the browse page
 * Sets up event listeners for room input, wall selection, and navigation
 * Initializes the breadcrumb and shows the room selection section by default
 */
function init() {
    // Room input
    const roomInput = $('roomInput');
    const enterRoomBtn = $('enterRoomBtn');
    
    if (enterRoomBtn) {
        enterRoomBtn.addEventListener('click', enterRoom);
    }
    
    if (roomInput) {
        // Add input validation - only allow URL-safe base64 characters and numbers
        roomInput.addEventListener('input', (e) => {
            const input = e.target;
            const value = input.value;
            // Allow: 0-9 (numbers), A-Z, a-z, -, _ (URL-safe base64 alphabet)
            const filtered = value.replace(/[^0-9A-Za-z\-_]/g, '');
            if (value !== filtered) {
                input.value = filtered;
            }
        });
        
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