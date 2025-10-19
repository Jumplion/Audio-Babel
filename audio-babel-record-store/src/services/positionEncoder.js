// src/services/positionEncoder.js
/**
 * Position encoder for mapping audio indexes to Library of Babel structure
 * 
 * Hierarchy:
 * - Room (Hexagon/Genre): Unlimited, identified by hash of content
 * - Wall (Artist): 1-4 per room
 * - Shelf (Longbox/Album): 1-20 per wall
 * - Track (Book): 1-32 per shelf
 * 
 * Total capacity per room: 4 × 20 × 32 = 2,560 tracks
 */

const WALLS_PER_ROOM = 4;
const SHELVES_PER_WALL = 20;
const TRACKS_PER_SHELF = 32;
const TRACKS_PER_ROOM = WALLS_PER_ROOM * SHELVES_PER_WALL * TRACKS_PER_SHELF; // 2,560

/**
 * Encode position data into the index
 * @param {string} base64Index - The base64-encoded audio index
 * @param {number} wall - Wall number (1-4)
 * @param {number} shelf - Shelf number (1-20)
 * @param {number} track - Track number (1-32)
 * @returns {object} Position metadata
 */
function encodePosition(base64Index, wall, shelf, track) {
    // Validate inputs
    if (wall < 1 || wall > WALLS_PER_ROOM) {
        throw new Error(`Wall must be between 1 and ${WALLS_PER_ROOM}`);
    }
    if (shelf < 1 || shelf > SHELVES_PER_WALL) {
        throw new Error(`Shelf must be between 1 and ${SHELVES_PER_WALL}`);
    }
    if (track < 1 || track > TRACKS_PER_SHELF) {
        throw new Error(`Track must be between 1 and ${TRACKS_PER_SHELF}`);
    }

    // Calculate room identifier from the index hash
    const roomId = calculateRoomId(base64Index);
    
    // Calculate linear position within room (0-indexed internally)
    const linearPosition = 
        ((wall - 1) * SHELVES_PER_WALL * TRACKS_PER_SHELF) +
        ((shelf - 1) * TRACKS_PER_SHELF) +
        (track - 1);

    return {
        roomId,
        wall,
        shelf,
        track,
        linearPosition,
        address: formatAddress(roomId, wall, shelf, track)
    };
}

/**
 * Decode position from an index
 * @param {string} base64Index - The base64-encoded audio index
 * @param {number} linearPosition - Linear position within room (0-2559)
 * @returns {object} Position metadata
 */
function decodePosition(base64Index, linearPosition) {
    if (linearPosition < 0 || linearPosition >= TRACKS_PER_ROOM) {
        throw new Error(`Linear position must be between 0 and ${TRACKS_PER_ROOM - 1}`);
    }

    // Calculate hierarchical position from linear position
    const wall = Math.floor(linearPosition / (SHELVES_PER_WALL * TRACKS_PER_SHELF)) + 1;
    const remainderAfterWall = linearPosition % (SHELVES_PER_WALL * TRACKS_PER_SHELF);
    const shelf = Math.floor(remainderAfterWall / TRACKS_PER_SHELF) + 1;
    const track = (remainderAfterWall % TRACKS_PER_SHELF) + 1;

    const roomId = calculateRoomId(base64Index);

    return {
        roomId,
        wall,
        shelf,
        track,
        linearPosition,
        address: formatAddress(roomId, wall, shelf, track)
    };
}

/**
 * Calculate room ID from the base64 index using a hash function
 * @param {string} base64Index - The base64-encoded audio index
 * @returns {string} Room identifier
 */
function calculateRoomId(base64Index) {
    // Use a simple hash of the first portion of the index to determine room
    // This ensures the same audio content always maps to the same room
    let hash = 0;
    const indexPrefix = base64Index.substring(0, Math.min(16, base64Index.length));
    
    for (let i = 0; i < indexPrefix.length; i++) {
        const char = indexPrefix.charCodeAt(i);
        hash = ((hash << 5) - hash) + char;
        hash = hash & hash; // Convert to 32-bit integer
    }
    
    // Convert to positive hex string
    const roomHash = (Math.abs(hash) >>> 0).toString(16).padStart(8, '0');
    return `room_${roomHash}`;
}

/**
 * Calculate position within room from audio index bytes
 * Uses the index content to deterministically assign a position
 * @param {string} base64Index - The base64-encoded audio index
 * @returns {number} Linear position (0-2559)
 */
function calculateLinearPosition(base64Index) {
    // Hash the entire index to get a deterministic position
    let hash = 0;
    for (let i = 0; i < base64Index.length; i++) {
        const char = base64Index.charCodeAt(i);
        hash = ((hash << 5) - hash) + char;
        hash = hash & hash;
    }
    
    // Map hash to position range (0-2559)
    return Math.abs(hash) % TRACKS_PER_ROOM;
}

/**
 * Format a human-readable address
 * @param {string} roomId - Room identifier
 * @param {number} wall - Wall number
 * @param {number} shelf - Shelf number
 * @param {number} track - Track number
 * @returns {string} Formatted address
 */
function formatAddress(roomId, wall, shelf, track) {
    return `${roomId}:W${wall}:S${shelf.toString().padStart(2, '0')}:T${track.toString().padStart(2, '0')}`;
}

/**
 * Parse an address string back into components
 * @param {string} address - Formatted address string
 * @returns {object} Parsed components
 */
function parseAddress(address) {
    const match = address.match(/^(room_[a-f0-9]+):W(\d+):S(\d+):T(\d+)$/);
    if (!match) {
        throw new Error('Invalid address format');
    }
    
    return {
        roomId: match[1],
        wall: parseInt(match[2], 10),
        shelf: parseInt(match[3], 10),
        track: parseInt(match[4], 10)
    };
}

/**
 * Get the full position metadata for an index
 * @param {string} base64Index - The base64-encoded audio index
 * @returns {object} Complete position metadata
 */
function getPositionFromIndex(base64Index) {
    const linearPosition = calculateLinearPosition(base64Index);
    return decodePosition(base64Index, linearPosition);
}

/**
 * Check if a position is valid
 * @param {number} wall - Wall number
 * @param {number} shelf - Shelf number
 * @param {number} track - Track number
 * @returns {boolean} True if valid
 */
function isValidPosition(wall, shelf, track) {
    return wall >= 1 && wall <= WALLS_PER_ROOM &&
           shelf >= 1 && shelf <= SHELVES_PER_WALL &&
           track >= 1 && track <= TRACKS_PER_SHELF;
}

export {
    WALLS_PER_ROOM,
    SHELVES_PER_WALL,
    TRACKS_PER_SHELF,
    TRACKS_PER_ROOM,
    encodePosition,
    decodePosition,
    calculateRoomId,
    calculateLinearPosition,
    formatAddress,
    parseAddress,
    getPositionFromIndex,
    isValidPosition
};
