// browse.js - Hierarchical navigation through the Record Shop library
import { getWasmModule } from '../core/wasmModule.js';
import { buildResultForIndex } from '../utils/resultBuilder.js';
import { indexToBase64 } from '../utils/base64.js';
import { showValidationError, handleError } from '../utils/errorHandler.js';
import { handleJsonResponse, cleanupResultHandler } from '../core/resultDisplay.js';
import { filterToBase64UrlChars } from '../utils/validationUtils.js';

// Library hierarchy constants — loaded from WASM at init time (R5).
// Fallbacks match the C++ LibraryConstants defaults.
let TRACKS_PER_ALBUM = 15;
let ALBUMS_PER_SHELF = 32;
let SHELVES_PER_WALL = 5;
let WALLS_PER_ROOM = 4;

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
            goToRoom(e);
        });
        parts.push(link);
    }
    
    if (navState.wall !== null) {
        const link = document.createElement('a');
        link.href = '#';
        link.textContent = `Wall ${navState.wall}`;
        link.addEventListener('click', (e) => {
            e.preventDefault();
            goToWall(e);
        });
        parts.push(link);
    }
    
    if (navState.shelf !== null) {
        const link = document.createElement('a');
        link.href = '#';
        link.textContent = `Shelf ${navState.shelf}`;
        link.addEventListener('click', (e) => {
            e.preventDefault();
            goToShelf(e);
        });
        parts.push(link);
    }
    
    if (navState.album !== null) {
        const link = document.createElement('a');
        link.href = '#';
        link.textContent = `Album ${navState.album}`;
        link.addEventListener('click', (e) => {
            e.preventDefault();
            goToAlbum(e);
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

// Sections that play a "zoom in to reveal" transition when they become visible.
// trackSection uses a slightly different variant (album lid tilting open).
const REVEAL_VARIANTS = {
    wallSection: null,
    shelfSection: null,
    albumSection: null,
    trackSection: 'album-open'
};

/**
 * Play the zoom-in reveal animation on a freshly-shown section.
 * If `originEvent` carries click coordinates, the animation's transform-origin
 * is set to that point so the new section appears to grow out of whatever was
 * just clicked (the wall segment / shelf / album button); otherwise it zooms
 * in from its own center (e.g. keyboard submission, breadcrumb back-nav).
 * @param {HTMLElement} el - Section element to animate
 * @param {MouseEvent|null} [originEvent] - Click event that triggered the navigation
 * @param {string|null} [variant] - Optional animation variant class (e.g. 'album-open')
 */
function triggerZoomReveal(el, originEvent = null, variant = null) {
    if (!el) return;

    if (originEvent && typeof originEvent.clientX === 'number') {
        const rect = el.getBoundingClientRect();
        const x = rect.width ? ((originEvent.clientX - rect.left) / rect.width) * 100 : 50;
        const y = rect.height ? ((originEvent.clientY - rect.top) / rect.height) * 100 : 50;
        el.style.setProperty('--reveal-x', `${Math.min(100, Math.max(0, x))}%`);
        el.style.setProperty('--reveal-y', `${Math.min(100, Math.max(0, y))}%`);
    } else {
        el.style.removeProperty('--reveal-x');
        el.style.removeProperty('--reveal-y');
    }

    // Restart the animation even if it's already mid-flight (e.g. rapid clicks)
    el.classList.remove('zoom-reveal', 'album-open');
    void el.offsetWidth; // force reflow
    el.classList.add('zoom-reveal');
    if (variant) el.classList.add(variant);
}

/**
 * Show only the specified section, hiding all others
 * @param {string} sectionId - ID of section to display
 * @param {MouseEvent|null} [originEvent] - Click event that triggered the navigation (for the reveal animation)
 */
function showSection(sectionId, originEvent = null) {
    const sections = ['roomSection', 'wallSection', 'shelfSection', 'albumSection', 'trackSection', 'resultSection'];
    sections.forEach(id => {
        const el = $(id);
        if (!el) return;

        const wasVisible = el.style.display === 'block';
        const willBeVisible = (id === sectionId);
        el.style.display = willBeVisible ? 'block' : 'none';

        if (willBeVisible && !wasVisible && Object.prototype.hasOwnProperty.call(REVEAL_VARIANTS, id)) {
            triggerZoomReveal(el, originEvent, REVEAL_VARIANTS[id]);
        }
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
 * @param {MouseEvent|null} [originEvent] - Click event that triggered the navigation
 */
function navigateToLevel(section, clearLevels, renderFn = null, originEvent = null) {
    clearNavLevels(clearLevels);
    showSection(section, originEvent);
    if (renderFn) renderFn();
    updateBreadcrumb();
}

/**
 * Navigate to room selection
 */
function goToRoom(originEvent = null) {
    clearNavLevels(['wall', 'shelf', 'album', 'track']);
    showSection('roomSection');
    showSection('wallSection', originEvent);
    updateBreadcrumb();
}

/**
 * Navigate to wall selection
 */
function goToWall(originEvent = null) {
    navigateToLevel('wallSection', ['shelf', 'album', 'track'], null, originEvent);
}

/**
 * Navigate to shelf selection
 */
function goToShelf(originEvent = null) {
    navigateToLevel('shelfSection', ['album', 'track'], renderShelves, originEvent);
}

/**
 * Navigate to album selection
 */
function goToAlbum(originEvent = null) {
    navigateToLevel('albumSection', ['track'], renderAlbums, originEvent);
}

/**
 * Enter a room by ID or base64 string
 * Validates input and converts numeric room IDs to base64 format
 * @param {MouseEvent|null} [originEvent] - Click event that triggered the navigation
 */
function enterRoom(originEvent = null) {
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

    showSection('wallSection', originEvent);
    updateBreadcrumb();
}

/**
 * Select a wall and navigate to shelf selection
 * @param {number} wallNum - Wall number (0-3)
 * @param {MouseEvent|null} [originEvent] - Click event that triggered the navigation
 */
function selectWall(wallNum, originEvent = null) {
    if (navState.room === null) return;

    navState.wall = wallNum;
    clearNavLevels(['shelf', 'album', 'track']);

    showSection('shelfSection', originEvent);
    renderShelves();
    updateBreadcrumb();
}

/**
 * Render numbered buttons in a container
 * @param {string} containerId - ID of container element
 * @param {number} count - Number of buttons to create
 * @param {string} className - CSS class for buttons
 * @param {Function} clickHandler - Click handler function, called as clickHandler(index, clickEvent)
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
        btn.addEventListener('click', (e) => clickHandler(i, e));
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
 * @param {MouseEvent|null} [originEvent] - Click event that triggered the navigation
 */
function selectShelf(shelfNum, originEvent = null) {
    if (navState.room === null || navState.wall === null) return;

    navState.shelf = shelfNum;
    clearNavLevels(['album', 'track']);

    showSection('albumSection', originEvent);
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
 * @param {MouseEvent|null} [originEvent] - Click event that triggered the navigation
 */
function selectAlbum(albumNum, originEvent = null) {
    if (navState.room === null || navState.wall === null || navState.shelf === null) return;

    navState.album = albumNum;
    clearNavLevels(['track']);

    showSection('trackSection', originEvent);
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
 * @param {MouseEvent|null} [originEvent] - Click event that triggered the navigation (unused; resultSection isn't animated)
 */
async function selectTrack(trackNum, originEvent = null) {
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
        const base64Index = wasm.module.reconstructIndex(
            navState.room.toString(),
            validatedPos.wall,
            validatedPos.shelf,
            validatedPos.album,
            validatedPos.track
        );
        console.log('Position index:', base64Index?.substring(0, 50) + '...');

        if (!base64Index) {
            throw new Error('Failed to reconstruct position index');
        }
        // On failure, reconstructIndex returns a JSON error object: {"error":"..."}
        if (base64Index.startsWith('{')) {
            const errResult = JSON.parse(base64Index);
            throw new Error(errResult.error || 'Failed to reconstruct position index');
        }

        // The index from reconstructIndex IS the full real index — no header to add.
        // Fetch its metadata/audio and render through the shared result handler.
        const result = await buildResultForIndex(wasm, base64Index);
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
    // Load library hierarchy constants from WASM (R5)
    getWasmModule().then(wasm => {
        try {
            const c = JSON.parse(wasm.module.getLibraryConstants());
            TRACKS_PER_ALBUM = c.tracksPerAlbum;
            ALBUMS_PER_SHELF = c.albumsPerShelf;
            SHELVES_PER_WALL = c.shelvesPerWall;
            WALLS_PER_ROOM   = c.wallsPerRoom;
        } catch (e) {
            console.warn('Could not load library constants from WASM, using defaults', e);
        }
    });

    // Room input
    const roomInput = $('roomInput');
    const enterRoomBtn = $('enterRoomBtn');
    
    if (enterRoomBtn) {
        enterRoomBtn.addEventListener('click', (e) => enterRoom(e));
    }
    
    if (roomInput) {
        // Add input validation - only allow URL-safe base64 characters and numbers
        roomInput.addEventListener('input', (e) => {
            const input = e.target;
            const value = input.value;
            const filtered = filterToBase64UrlChars(value);
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
        wall.addEventListener('click', (e) => selectWall(wallNum, e));
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