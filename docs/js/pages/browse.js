// browse.js - Hierarchical navigation through the Record Shop library
import { getWasmModule } from '../core/wasmModule.js';
import { buildResultForIndex } from '../utils/resultBuilder.js';
import { indexToBase64 } from '../utils/base64.js';
import { showValidationError, handleError } from '../utils/errorHandler.js';
import { handleJsonResponse, cleanupResultHandler } from '../core/resultDisplay.js?v=3';
import { filterToBase64UrlChars } from '../utils/validationUtils.js';
import { consumeFindInLibraryTarget } from '../utils/findInLibrary.js';

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
  track: null,
  // Cosmetic names for the currently-rendered sibling group at each level,
  // cached as they're fetched so the breadcrumb can show them without an
  // extra WASM round-trip. shelfNames are artist names (shelf level),
  // matching docs/browse.html's wall=label/shelf=artist hierarchy. The
  // internal field names stay wall/shelf for brevity; user-facing text
  // (breadcrumb, section headers, hexagon tooltips) shows "Label"/"Artist".
  shelfNames: null,
  albumNames: null,
  trackNames: null,
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
    console.warn(
      `Position value clamped: ${name} ${original} → ${clamped} (valid range: 0-${max})`
    );
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
    track: clampPositionValue(track, TRACKS_PER_ALBUM - 1, 'track'),
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
    link.className = 'breadcrumb-room';
    link.textContent = `Room ${navState.room}`;
    link.title = link.textContent;
    link.addEventListener('click', (e) => {
      e.preventDefault();
      goToRoom(e);
    });
    parts.push(link);
  }

  if (navState.wall !== null) {
    const link = document.createElement('a');
    link.href = '#';
    link.textContent = `Label ${navState.wall}`;
    link.addEventListener('click', (e) => {
      e.preventDefault();
      goToWall(e);
    });
    parts.push(link);
  }

  if (navState.shelf !== null) {
    const link = document.createElement('a');
    link.href = '#';
    const shelfName = navState.shelfNames?.[navState.shelf];
    link.textContent = shelfName
      ? `Artist ${navState.shelf} — ${shelfName}`
      : `Artist ${navState.shelf}`;
    link.addEventListener('click', (e) => {
      e.preventDefault();
      goToShelf(e);
    });
    parts.push(link);
  }

  if (navState.album !== null) {
    const link = document.createElement('a');
    link.href = '#';
    const albumName = navState.albumNames?.[navState.album];
    link.textContent = albumName
      ? `Album ${navState.album} — ${albumName}`
      : `Album ${navState.album}`;
    link.addEventListener('click', (e) => {
      e.preventDefault();
      goToAlbum(e);
    });
    parts.push(link);
  }

  if (navState.track !== null) {
    const span = document.createElement('span');
    const trackName = navState.trackNames?.[navState.track];
    span.textContent = trackName
      ? `Track ${navState.track} — ${trackName}`
      : `Track ${navState.track}`;
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
// trackSection's own "opening the gatefold" animation (see openAlbumGatefold)
// provides its dramatic reveal, so it just uses the plain zoom-in here.
const REVEAL_VARIANTS = {
  wallSection: null,
  shelfSection: null,
  albumSection: null,
  trackSection: null,
};

/**
 * Fade the selected track's info/waveform in on the sleeve, replacing the
 * album art. Toggles `display` first so the opacity transition can run (a
 * class added in the same frame as `display: none -> flex` won't animate).
 */
function showTrackDetail() {
  const panel = $('trackDetail');
  if (!panel) return;
  panel.style.display = 'flex';
  requestAnimationFrame(() => {
    requestAnimationFrame(() => panel.classList.add('visible'));
  });
}

/**
 * Hide the track detail panel, revealing the album art again (e.g. when a
 * new album is opened, so a previous track's info doesn't linger before a
 * track is chosen).
 */
function hideTrackDetail() {
  const panel = $('trackDetail');
  if (!panel) return;
  panel.classList.remove('visible');
  panel.style.display = 'none';
}

// How long the outgoing section's zoom-out plays before it's hidden and the
// incoming section zooms in. Must match the .zoom-out animation duration in
// browse.css (0.2s) so the swap lands as the fade-out completes.
const OUTGOING_ZOOM_MS = 200;

// Holds the pending swap timer for an in-flight push-through transition so a
// rapid follow-up navigation can cancel it instead of stranding a section
// mid-zoom.
let revealTimer = null;

/**
 * Point a section's animation transform-origin at the spot that was clicked.
 * Maps the click's viewport coordinates into the element's own box (0–100%) so
 * the zoom appears to emanate from / dive into whatever was just clicked (the
 * wall segment / shelf / album button). With no click coordinates (keyboard
 * submission, breadcrumb back-nav) the origin is cleared, defaulting to center.
 * @param {HTMLElement} el - Section element being animated
 * @param {MouseEvent|null} originEvent - Click event that triggered the navigation
 */
function setRevealOrigin(el, originEvent) {
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
}

/**
 * Play the zoom-in reveal animation on a freshly-shown section, growing it out
 * of the clicked point (see setRevealOrigin).
 * @param {HTMLElement} el - Section element to animate
 * @param {MouseEvent|null} [originEvent] - Click event that triggered the navigation
 * @param {string|null} [variant] - Optional animation variant class (e.g. 'album-open')
 */
function triggerZoomReveal(el, originEvent = null, variant = null) {
  if (!el) return;
  setRevealOrigin(el, originEvent);

  // Restart the animation even if it's already mid-flight (e.g. rapid clicks)
  el.classList.remove('zoom-reveal', 'zoom-out', 'album-open');
  void el.offsetWidth; // force reflow
  el.classList.add('zoom-reveal');
  if (variant) el.classList.add(variant);
}

/**
 * Play the zoom-out animation on the section being left, so it enlarges toward
 * the viewer and fades — the "push into the clicked element" half of the
 * transition. Origin is the same clicked point as the incoming zoom-in.
 * @param {HTMLElement} el - Section element being hidden
 * @param {MouseEvent|null} [originEvent] - Click event that triggered the navigation
 */
function triggerZoomOut(el, originEvent = null) {
  if (!el) return;
  setRevealOrigin(el, originEvent);

  el.classList.remove('zoom-reveal', 'zoom-out', 'album-open');
  void el.offsetWidth; // force reflow
  el.classList.add('zoom-out');
}

/**
 * Show only the specified section, hiding all others
 * @param {string} sectionId - ID of section to display
 * @param {MouseEvent|null} [originEvent] - Click event that triggered the navigation (for the reveal animation)
 */
function showSection(sectionId, originEvent = null) {
  const sections = ['roomSection', 'wallSection', 'shelfSection', 'albumSection', 'trackSection'];

  // Cancel any in-flight push-through so rapid navigation doesn't leave a
  // half-zoomed section behind.
  if (revealTimer) {
    clearTimeout(revealTimer);
    revealTimer = null;
  }

  const canAnimate = (id) => Object.prototype.hasOwnProperty.call(REVEAL_VARIANTS, id);
  const target = $(sectionId);

  // The animatable section currently on screen that we're leaving (if any).
  const outgoing = sections
    .map((id) => $(id))
    .find((el) => el && el.id !== sectionId && el.style.display === 'block');

  // Hide every non-target section and reveal the target with its zoom-in.
  const finishSwap = () => {
    sections.forEach((id) => {
      const el = $(id);
      if (!el || id === sectionId) return;
      el.style.display = 'none';
      el.classList.remove('zoom-reveal', 'zoom-out', 'album-open');
    });
    if (target) {
      target.style.display = 'block';
      if (canAnimate(sectionId)) {
        triggerZoomReveal(target, originEvent, REVEAL_VARIANTS[sectionId]);
      }
    }
  };

  // Full push-through only when both ends animate (e.g. label→artist→album→
  // track). Otherwise (initial load, room form, reduced fallbacks) swap at
  // once so there's no needless delay.
  if (outgoing && canAnimate(outgoing.id) && canAnimate(sectionId)) {
    triggerZoomOut(outgoing, originEvent);
    revealTimer = setTimeout(() => {
      revealTimer = null;
      finishSwap();
    }, OUTGOING_ZOOM_MS);
  } else {
    finishSwap();
  }

  // Clean up the result handler (wavesurfer instance) when leaving track
  // section, since its info panel is the only place results are shown now
  if (sectionId !== 'trackSection') {
    cleanupResultHandler();
  }
}

/**
 * Clear navigation state from a given level downward
 * @param {string[]} levelsToClear - Navigation levels to clear (e.g., ['wall', 'shelf', 'album', 'track'])
 */
function clearNavLevels(levelsToClear) {
  levelsToClear.forEach((level) => {
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
    room = '';
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
  updateWallTooltips();
}

/**
 * Label each wall hexagon segment with its label name: a native title
 * tooltip plus an in-hexagon text label that fades in on hover/focus, the
 * same opacity-reveal treatment .album-number gets for albums (see
 * .wall-genre-label in browse.css). Fire-and-forget — the hexagon is fully
 * usable before the names arrive.
 */
async function updateWallTooltips() {
  if (navState.room === null) return;
  try {
    const wasm = await getWasmModule();
    const raw = JSON.parse(wasm.module.getGenreNames(navState.room.toString()));
    if (!Array.isArray(raw)) throw new Error(raw.error || 'Failed to load genre names');
    document.querySelectorAll('.wall').forEach((wall) => {
      const wallNum = parseInt(wall.dataset.wall, 10);
      if (!raw[wallNum]) return;
      wall.setAttribute('title', raw[wallNum]);
      const label = document.querySelector(`.wall-genre-label[data-wall="${wallNum}"]`);
      if (label) label.textContent = raw[wallNum];
    });
  } catch (err) {
    console.error('Failed to load genre names', err);
  }
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
 * Render the shelves ("longboxes") for the current wall
 * Fetches each shelf's artist name (shelf level, per docs/browse.html's
 * wall=label/shelf=artist hierarchy) and labels the buttons with it instead
 * of a bare index.
 */
async function renderShelves() {
  const container = $('shelvesContainer');
  if (container) container.innerHTML = ''; // clear stale buttons while names load

  let names = [];
  try {
    const wasm = await getWasmModule();
    const raw = JSON.parse(wasm.module.getArtistNames(navState.room.toString(), navState.wall));
    if (!Array.isArray(raw)) throw new Error(raw.error || 'Failed to load shelf names');
    names = raw;
  } catch (err) {
    console.error('Failed to load shelf (artist) names', err);
  }

  navState.shelfNames = names;
  renderButtons(
    'shelvesContainer',
    SHELVES_PER_WALL,
    'shelf-btn',
    selectShelf,
    (i) => names[i] ?? `${i}`
  );
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
 * Creates album "spine" buttons (0-31); each shows its generated album name
 * only on hover/focus (see .album-number in browse.css), evoking a long box
 * of records flipped through sideways.
 */
async function renderAlbums() {
  const container = $('albumsContainer');
  if (!container) return;

  container.innerHTML = ''; // clear stale buttons while names load

  let names = [];
  try {
    const wasm = await getWasmModule();
    const raw = JSON.parse(
      wasm.module.getAlbumNames(navState.room.toString(), navState.wall, navState.shelf)
    );
    if (!Array.isArray(raw)) throw new Error(raw.error || 'Failed to load album names');
    names = raw;
  } catch (err) {
    console.error('Failed to load album names', err);
  }
  navState.albumNames = names;

  container.innerHTML = '';
  for (let i = 0; i < ALBUMS_PER_SHELF; i++) {
    const name = names[i] ?? `${i}`;

    const btn = document.createElement('button');
    btn.className = 'album-btn';
    btn.setAttribute('aria-label', `Album ${name}`);

    const labelSpan = document.createElement('span');
    labelSpan.className = 'album-number';
    labelSpan.textContent = name;
    btn.appendChild(labelSpan);

    btn.addEventListener('click', (e) => selectAlbum(i, e));
    container.appendChild(btn);
  }
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
  openAlbumGatefold(albumNum);
  updateBreadcrumb();
}

/**
 * Render the tracks for the current album
 * Fetches each track's generated name and labels the buttons with it
 * instead of a bare index.
 */
async function renderTracks() {
  const container = $('tracksContainer');
  if (container) container.innerHTML = ''; // clear stale buttons while names load

  let names = [];
  try {
    const wasm = await getWasmModule();
    const raw = JSON.parse(
      wasm.module.getTrackNames(
        navState.room.toString(),
        navState.wall,
        navState.shelf,
        navState.album
      )
    );
    if (!Array.isArray(raw)) throw new Error(raw.error || 'Failed to load track names');
    names = raw;
  } catch (err) {
    console.error('Failed to load track names', err);
  }

  navState.trackNames = names;
  renderButtons(
    'tracksContainer',
    TRACKS_PER_ALBUM,
    'track-btn',
    selectTrack,
    (i) => names[i] ?? `${i}`
  );
}

/**
 * Build a "stand-in" full index for an album using track 0.
 * Cover art only exists at the full index level (room+wall+shelf+album+track),
 * not per-album, so this borrows track 0 purely to generate art to show on
 * the closed cover before any specific track has been chosen.
 * @param {Object} wasm - Initialized IndexWasm instance
 * @param {number} albumNum - Album number (0-31)
 * @returns {string} Base64 index
 */
function buildAlbumStandInIndex(wasm, albumNum) {
  const pos = validatePositionValues(navState.wall, navState.shelf, albumNum, 0);
  const result = wasm.module.reconstructIndex(
    navState.room.toString(),
    pos.wall,
    pos.shelf,
    pos.album,
    pos.track
  );
  if (typeof result === 'string' && result.startsWith('{')) {
    const err = JSON.parse(result);
    throw new Error(err.error || 'Failed to build stand-in index for album cover');
  }
  return result;
}

/**
 * Fetch and display the generated cover art + caption for an album on the
 * gatefold's front cover face.
 * @param {number} albumNum - Album number (0-31)
 */
async function loadGatefoldCover(albumNum) {
  const coverArt = $('coverArt');
  const coverCaption = $('coverCaption');
  if (!coverArt || !coverCaption) return;

  try {
    const wasm = await getWasmModule();
    const indexBase64 = buildAlbumStandInIndex(wasm, albumNum);
    const metadata = JSON.parse(wasm.module.getMetadata(indexBase64));
    if (metadata.error) throw new Error(metadata.error);

    coverArt.src = 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(metadata.cover);
    coverCaption.textContent = `${metadata.artist} — ${metadata.album}`;
  } catch (err) {
    console.error('Failed to load album cover art', err);
    coverArt.src = '';
    coverCaption.textContent = '';
  }
}

/**
 * Reset the gatefold to its closed state (inner leaf and record tucked away).
 */
function resetGatefold() {
  const stage = $('gatefoldStage');
  if (stage) stage.classList.remove('open');
  hideTrackDetail();
}

/**
 * Populate the gatefold for a newly-selected album (cover art + track list),
 * then swing it open shortly after so the reveal reads as "opening the
 * album you just picked" rather than a plain content swap.
 * @param {number} albumNum - Album number (0-31)
 */
async function openAlbumGatefold(albumNum) {
  resetGatefold();
  renderTracks();
  await loadGatefoldCover(albumNum);

  setTimeout(() => {
    const stage = $('gatefoldStage');
    if (stage) stage.classList.add('open');
  }, 350);
}

/**
 * Select a track and display its audio content
 * Validates that all parent levels have been selected. Stays on the track
 * section — the result (metadata + waveform) fades in on the sleeve, in
 * place of the album art, rather than replacing the track selector.
 * @param {number} trackNum - Track number (0-14)
 * @param {MouseEvent|null} [originEvent] - Click event that triggered the navigation (unused)
 */
async function selectTrack(trackNum, originEvent = null) {
  if (
    navState.room === null ||
    navState.wall === null ||
    navState.shelf === null ||
    navState.album === null
  )
    return;

  navState.track = trackNum;
  updateBreadcrumb();

  showTrackDetail();
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
      track: navState.track,
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
 * Wait for the given number of milliseconds.
 * @param {number} ms - Milliseconds to wait
 * @returns {Promise<void>}
 */
function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

/**
 * Step through room -> genre -> artist -> album -> track one level at a
 * time, replaying the same selection functions a clicking user would
 * trigger so the existing reveal/gatefold animations play at each step.
 * Used by the Search page's "Find in Library" button (see findInLibrary.js)
 * to land a result's position directly inside the Browse hierarchy.
 * @param {{room: string, wall: number, shelf: number, album: number, track: number}} target
 */
async function autoNavigateToPosition(target) {
  showSection('roomSection');

  const input = $('roomInput');
  if (input) input.value = target.room ?? '';
  await delay(400);

  enterRoom();
  await delay(700);

  selectWall(target.wall);
  await delay(700);

  selectShelf(target.shelf);
  await delay(700);

  selectAlbum(target.album);
  await delay(900);

  await selectTrack(target.track);
}

/**
 * Initialize the browse page
 * Sets up event listeners for room input, wall selection, and navigation
 * Initializes the breadcrumb and shows the room selection section by default
 */
function init() {
  // Load library hierarchy constants from WASM (R5)
  getWasmModule().then((wasm) => {
    try {
      const c = JSON.parse(wasm.module.getLibraryConstants());
      TRACKS_PER_ALBUM = c.tracksPerAlbum;
      ALBUMS_PER_SHELF = c.albumsPerShelf;
      SHELVES_PER_WALL = c.shelvesPerWall;
      WALLS_PER_ROOM = c.wallsPerRoom;
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
  walls.forEach((wall) => {
    const wallNum = parseInt(wall.dataset.wall);
    wall.addEventListener('click', (e) => selectWall(wallNum, e));
    wall.addEventListener('mouseenter', () => wall.classList.add('hover'));
    wall.addEventListener('mouseleave', () => wall.classList.remove('hover'));
  });

  // Clicking the closed cover opens it (in case the auto-open is skipped,
  // e.g. reduced-motion users landing mid-transition)
  const coverRight = $('coverRight');
  if (coverRight) {
    coverRight.addEventListener('click', () => {
      const stage = $('gatefoldStage');
      if (stage) stage.classList.add('open');
    });
  }

  // Initialize breadcrumb
  updateBreadcrumb();

  // If the Search page handed off a "Find in Library" target, auto-walk
  // to it; otherwise show room selection as usual.
  const pendingTarget = consumeFindInLibraryTarget();
  if (pendingTarget) {
    autoNavigateToPosition(pendingTarget);
  } else {
    showSection('roomSection');
  }
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
