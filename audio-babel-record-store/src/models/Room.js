import { WALLS_PER_ROOM } from '../services/positionEncoder.js';

class Room {
    constructor(roomId, genreLabel) {
        this.roomId = roomId; // Unique identifier (e.g., "room_abc123")
        this.genreLabel = genreLabel; // Display label for the genre
        this.walls = []; // Array to hold walls (max 4)
        
        // Initialize with 4 empty walls
        for (let i = 1; i <= WALLS_PER_ROOM; i++) {
            this.walls.push(null);
        }
    }

    /**
     * Add or update a wall at a specific position
     * @param {number} wallNumber - Wall number (1-4)
     * @param {Wall} wall - Wall object
     */
    setWall(wallNumber, wall) {
        if (wallNumber < 1 || wallNumber > WALLS_PER_ROOM) {
            throw new Error(`Wall number must be between 1 and ${WALLS_PER_ROOM}`);
        }
        this.walls[wallNumber - 1] = wall;
    }

    /**
     * Get a wall at a specific position
     * @param {number} wallNumber - Wall number (1-4)
     * @returns {Wall|null} Wall object or null if empty
     */
    getWall(wallNumber) {
        if (wallNumber < 1 || wallNumber > WALLS_PER_ROOM) {
            throw new Error(`Wall number must be between 1 and ${WALLS_PER_ROOM}`);
        }
        return this.walls[wallNumber - 1];
    }

    /**
     * Get all walls
     * @returns {Array} Array of walls (may contain nulls)
     */
    getWalls() {
        return this.walls;
    }

    /**
     * Get all non-null walls
     * @returns {Array} Array of walls (no nulls)
     */
    getPopulatedWalls() {
        return this.walls.filter(wall => wall !== null);
    }

    /**
     * Check if room is full
     * @returns {boolean} True if all walls are populated
     */
    isFull() {
        return this.walls.every(wall => wall !== null && wall.isFull());
    }

    /**
     * Get the total number of tracks in this room
     * @returns {number} Total track count
     */
    getTrackCount() {
        return this.walls.reduce((sum, wall) => {
            return sum + (wall ? wall.getTrackCount() : 0);
        }, 0);
    }

    toJSON() {
        return {
            roomId: this.roomId,
            genreLabel: this.genreLabel,
            walls: this.walls.map((wall, index) => ({
                wallNumber: index + 1,
                data: wall ? wall.toJSON() : null
            })),
            trackCount: this.getTrackCount(),
            isFull: this.isFull()
        };
    }
}

class Wall {
    constructor(artist) {
        this.artist = artist;
        this.shelves = []; // Array to hold shelves (albums)
    }

    addShelf(album) {
        const shelf = new Shelf(album);
        this.shelves.push(shelf);
    }

    getShelves() {
        return this.shelves;
    }

    toJSON() {
        return {
            artist: this.artist,
            shelves: this.shelves.map(shelf => shelf.toJSON())
        };
    }
}

class Shelf {
    constructor(album) {
        this.album = album;
        this.tracks = []; // Array to hold tracks
    }

    addTrack(track) {
        this.tracks.push(track);
    }

    getTracks() {
        return this.tracks;
    }

    toJSON() {
        return {
            album: this.album,
            tracks: this.tracks
        };
    }
}

class Track {
    constructor(title, duration) {
        this.title = title;
        this.duration = duration; // Duration in seconds
    }

    toJSON() {
        return {
            title: this.title,
            duration: this.duration
        };
    }
}

export { Room, Wall, Shelf, Track };