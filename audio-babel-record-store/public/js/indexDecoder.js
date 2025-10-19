// This file contains functions to decode the unique indexing system for genres, artists, albums, and tracks.

function decodeIndex(index) {
    // Assuming the index is a base64 URL-safe encoded string
    const decoded = atob(index.replace(/-/g, '+').replace(/_/g, '/'));
    const parts = decoded.split('|'); // Assuming '|' is used as a delimiter
    return {
        genre: parts[0] || 'Unknown Genre',
        artist: parts[1] || 'Unknown Artist',
        album: parts[2] || 'Unknown Album',
        track: parts[3] || 'Unknown Track'
    };
}

function encodeIndex(genre, artist, album, track) {
    const indexString = [genre, artist, album, track].join('|');
    return btoa(indexString).replace(/\+/g, '-').replace(/\//g, '_'); // URL-safe encoding
}

// Example usage
// const index = encodeIndex('Rock', 'Artist Name', 'Album Title', 'Track Title');
// const decoded = decodeIndex(index);