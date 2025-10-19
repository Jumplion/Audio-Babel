// src/services/indexParser.js
function parseIndex(index) {
    const parts = index.split('-');
    if (parts.length !== 4) {
        throw new Error('Invalid index format. Expected format: genre-artist-album-track');
    }
    return {
        genre: parts[0],
        artist: parts[1],
        album: parts[2],
        track: parts[3]
    };
}

function generateIndex(genre, artist, album, track) {
    return `${genre}-${artist}-${album}-${track}`;
}

function validateIndex(index) {
    const regex = /^[A-Za-z0-9]+(-[A-Za-z0-9]+){3}$/;
    if (!regex.test(index)) {
        throw new Error('Invalid index. Must match the format: genre-artist-album-track');
    }
    return true;
}

export { parseIndex, generateIndex, validateIndex };