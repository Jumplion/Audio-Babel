/**
 * positionEncoder.js
 * 
 * Handles encoding/decoding between base64 audio indexes and hierarchical positions
 * in the Record Shop library structure:
 * Room → Wall (4, representing genre) → Shelf (5, representing artist) → 
 * Album (32) → Track (15)
 */

// Library structure constants (matching C++ LibraryConstants)
export const TRACKS_PER_ALBUM = 15;
export const ALBUMS_PER_SHELF = 32;
export const SHELVES_PER_WALL = 5;
export const WALLS_PER_ROOM = 4;

export const ITEMS_PER_ALBUM = TRACKS_PER_ALBUM;                    // 15
export const ITEMS_PER_SHELF = ALBUMS_PER_SHELF * ITEMS_PER_ALBUM; // 480
export const ITEMS_PER_WALL = SHELVES_PER_WALL * ITEMS_PER_SHELF;  // 2,400
export const ITEMS_PER_ROOM = WALLS_PER_ROOM * ITEMS_PER_WALL;     // 9,600

/**
 * Represents a position in the library hierarchy
 */
export class LibraryPosition {
    constructor(room = 0n, wall = 0, shelf = 0, album = 0, track = 0) {
        this.room = BigInt(room);
        this.wall = wall;
        this.shelf = shelf;
        this.album = album;
        this.track = track;
    }

    toString() {
        return `Room ${this.room}, Wall ${this.wall}, Shelf ${this.shelf}, Album ${this.album}, Track ${this.track}`;
    }
}

/**
 * Calculate library position from a base64 index string
 * @param {string} base64Index - URL-safe base64 encoded index (no padding)
 * @returns {LibraryPosition}
 */
export function calculatePositionFromBase64(base64Index) {
    // Convert base64 to bytes
    const bytes = base64UrlToBytes(base64Index);
    
    // Convert bytes to BigInt (big-endian)
    let index = 0n;
    for (const byte of bytes) {
        index = (index << 8n) | BigInt(byte);
    }
    
    return calculateLibraryPosition(index);
}

/**
 * Calculate hierarchical position from an index using modular arithmetic
 * @param {BigInt} index - The audio index as a BigInt
 * @returns {LibraryPosition}
 */
export function calculateLibraryPosition(index) {
    const idx = BigInt(index);
    
    // Calculate room number (infinite rooms possible)
    const room = idx / BigInt(ITEMS_PER_ROOM);
    
    // Calculate position within the room
    let remainder = idx % BigInt(ITEMS_PER_ROOM);
    
    // Wall (0-3): 4 walls per room
    const wall = Number((remainder / BigInt(ITEMS_PER_WALL)) % BigInt(WALLS_PER_ROOM));
    
    remainder = remainder % BigInt(ITEMS_PER_WALL);
    
    // Shelf (0-4): 5 shelves per wall
    const shelf = Number((remainder / BigInt(ITEMS_PER_SHELF)) % BigInt(SHELVES_PER_WALL));
    
    remainder = remainder % BigInt(ITEMS_PER_SHELF);
    
    // Album (0-31): 32 albums per shelf
    const album = Number((remainder / BigInt(ITEMS_PER_ALBUM)) % BigInt(ALBUMS_PER_SHELF));
    
    // Track (0-14): 15 tracks per album
    const track = Number(remainder % BigInt(TRACKS_PER_ALBUM));
    
    return new LibraryPosition(room, wall, shelf, album, track);
}

/**
 * Reconstruct an index from a hierarchical position
 * @param {LibraryPosition} pos - The library position
 * @returns {BigInt} - The audio index
 */
export function reconstructIndexFromPosition(pos) {
    let index = BigInt(pos.room) * BigInt(ITEMS_PER_ROOM);
    index += BigInt(pos.wall) * BigInt(ITEMS_PER_WALL);
    index += BigInt(pos.shelf) * BigInt(ITEMS_PER_SHELF);
    index += BigInt(pos.album) * BigInt(ITEMS_PER_ALBUM);
    index += BigInt(pos.track);
    
    return index;
}

/**
 * Convert index BigInt to base64 URL-safe string (no padding)
 * @param {BigInt} index
 * @returns {string}
 */
export function indexToBase64(index) {
    const idx = BigInt(index);
    if (idx === 0n) return 'A'; // Special case for zero
    
    // Convert BigInt to bytes (big-endian)
    const bytes = [];
    let temp = idx;
    while (temp > 0n) {
        bytes.unshift(Number(temp & 0xFFn));
        temp = temp >> 8n;
    }
    
    return bytesToBase64Url(bytes);
}

/**
 * Convert bytes array to base64 URL-safe string (no padding)
 * @param {number[]} bytes
 * @returns {string}
 */
function bytesToBase64Url(bytes) {
    const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
    let result = '';
    let acc = 0;
    let accBits = 0;
    
    for (const byte of bytes) {
        acc = (acc << 8) | byte;
        accBits += 8;
        
        while (accBits >= 6) {
            accBits -= 6;
            const idx = (acc >> accBits) & 0x3F;
            result += alphabet[idx];
        }
    }
    
    if (accBits > 0) {
        const idx = (acc << (6 - accBits)) & 0x3F;
        result += alphabet[idx];
    }
    
    return result;
}

/**
 * Convert base64 URL-safe string to bytes array
 * @param {string} base64
 * @returns {number[]}
 */
function base64UrlToBytes(base64) {
    const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
    const lookup = {};
    for (let i = 0; i < alphabet.length; i++) {
        lookup[alphabet[i]] = i;
    }
    
    const bytes = [];
    let acc = 0;
    let accBits = 0;
    
    for (const char of base64) {
        if (!(char in lookup)) {
            throw new Error(`Invalid base64 character: ${char}`);
        }
        
        acc = (acc << 6) | lookup[char];
        accBits += 6;
        
        if (accBits >= 8) {
            accBits -= 8;
            bytes.push((acc >> accBits) & 0xFF);
        }
    }
    
    return bytes;
}

console.info('positionEncoder.js loaded');
