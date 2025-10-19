import { SHELVES_PER_WALL } from '../services/positionEncoder.js';

class Wall {
    constructor(wallNumber, artistLabel) {
        this.wallNumber = wallNumber; // Wall position (1-4)
        this.artistLabel = artistLabel; // Display label for the artist
        this.shelves = []; // Array to hold shelves (max 20)
        
        // Initialize with 20 empty shelves
        for (let i = 1; i <= SHELVES_PER_WALL; i++) {
            this.shelves.push(null);
        }
    }

    /**
     * Set a shelf at a specific position
     * @param {number} shelfNumber - Shelf number (1-20)
     * @param {Shelf} shelf - Shelf object
     */
    setShelf(shelfNumber, shelf) {
        if (shelfNumber < 1 || shelfNumber > SHELVES_PER_WALL) {
            throw new Error(`Shelf number must be between 1 and ${SHELVES_PER_WALL}`);
        }
        this.shelves[shelfNumber - 1] = shelf;
    }

    /**
     * Get a shelf at a specific position
     * @param {number} shelfNumber - Shelf number (1-20)
     * @returns {Shelf|null} Shelf object or null if empty
     */
    getShelf(shelfNumber) {
        if (shelfNumber < 1 || shelfNumber > SHELVES_PER_WALL) {
            throw new Error(`Shelf number must be between 1 and ${SHELVES_PER_WALL}`);
        }
        return this.shelves[shelfNumber - 1];
    }

    /**
     * Get all shelves
     * @returns {Array} Array of shelves (may contain nulls)
     */
    getShelves() {
        return this.shelves;
    }

    /**
     * Get all non-null shelves
     * @returns {Array} Array of shelves (no nulls)
     */
    getPopulatedShelves() {
        return this.shelves.filter(shelf => shelf !== null);
    }

    /**
     * Check if wall is full
     * @returns {boolean} True if all shelves are populated and full
     */
    isFull() {
        return this.shelves.every(shelf => shelf !== null && shelf.isFull());
    }

    /**
     * Get the total number of tracks on this wall
     * @returns {number} Total track count
     */
    getTrackCount() {
        return this.shelves.reduce((sum, shelf) => {
            return sum + (shelf ? shelf.getTrackCount() : 0);
        }, 0);
    }

    toJSON() {
        return {
            wallNumber: this.wallNumber,
            artistLabel: this.artistLabel,
            shelves: this.shelves.map((shelf, index) => ({
                shelfNumber: index + 1,
                data: shelf ? shelf.toJSON() : null
            })),
            trackCount: this.getTrackCount(),
            isFull: this.isFull()
        };
    }
}

export default Wall;