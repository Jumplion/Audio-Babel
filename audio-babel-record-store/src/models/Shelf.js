import { TRACKS_PER_SHELF } from '../services/positionEncoder.js';

class Shelf {
    constructor(shelfNumber, albumLabel) {
        this.shelfNumber = shelfNumber; // Shelf position (1-20)
        this.albumLabel = albumLabel;   // Display label for the album
        this.tracks = [];               // Array to hold tracks (max 32)
        
        // Initialize with 32 empty track slots
        for (let i = 1; i <= TRACKS_PER_SHELF; i++) {
            this.tracks.push(null);
        }
    }

    /**
     * Set a track at a specific position
     * @param {number} trackNumber - Track number (1-32)
     * @param {Track} track - Track object
     */
    setTrack(trackNumber, track) {
        if (trackNumber < 1 || trackNumber > TRACKS_PER_SHELF) {
            throw new Error(`Track number must be between 1 and ${TRACKS_PER_SHELF}`);
        }
        this.tracks[trackNumber - 1] = track;
    }

    /**
     * Get a track at a specific position
     * @param {number} trackNumber - Track number (1-32)
     * @returns {Track|null} Track object or null if empty
     */
    getTrack(trackNumber) {
        if (trackNumber < 1 || trackNumber > TRACKS_PER_SHELF) {
            throw new Error(`Track number must be between 1 and ${TRACKS_PER_SHELF}`);
        }
        return this.tracks[trackNumber - 1];
    }

    /**
     * Get all tracks
     * @returns {Array} Array of tracks (may contain nulls)
     */
    getTracks() {
        return this.tracks;
    }

    /**
     * Get all non-null tracks
     * @returns {Array} Array of tracks (no nulls)
     */
    getPopulatedTracks() {
        return this.tracks.filter(track => track !== null);
    }

    /**
     * Check if shelf is full
     * @returns {boolean} True if all track slots are populated
     */
    isFull() {
        return this.tracks.every(track => track !== null);
    }

    /**
     * Get the total number of populated tracks
     * @returns {number} Track count
     */
    getTrackCount() {
        return this.tracks.filter(track => track !== null).length;
    }

    toJSON() {
        return {
            shelfNumber: this.shelfNumber,
            albumLabel: this.albumLabel,
            tracks: this.tracks.map((track, index) => ({
                trackNumber: index + 1,
                data: track ? track.toJSON() : null
            })),
            trackCount: this.getTrackCount(),
            isFull: this.isFull()
        };
    }
}

export default Shelf;