class Track {
    constructor(index, position, metadata, duration, coverSvg = null) {
        this.index = index;           // Base64 URL-safe index (unique identifier)
        this.position = position;     // Position object { roomId, wall, shelf, track, address }
        this.metadata = metadata;     // Metadata object { genre, artist, album, track }
        this.duration = duration;     // Duration in seconds
        this.coverSvg = coverSvg;     // SVG cover art (optional)
        
        // Derived display properties
        this.trackNumber = position.track;
        this.shelfNumber = position.shelf;
        this.wallNumber = position.wall;
        this.roomId = position.roomId;
        this.address = position.address;
    }

    /**
     * Get formatted track details for display
     * @returns {string} Formatted track information
     */
    getTrackDetails() {
        return `Track ${this.trackNumber} on Shelf ${this.shelfNumber}, Wall ${this.wallNumber} in ${this.roomId} - Duration: ${this.formatDuration()}`;
    }

    /**
     * Get metadata labels
     * @returns {object} Formatted metadata
     */
    getMetadataLabels() {
        return {
            genre: `Genre: ${this.metadata.genre}`,
            artist: `Artist: ${this.metadata.artist}`,
            album: `Album: ${this.metadata.album}`,
            track: `Track: ${this.metadata.track}`
        };
    }

    /**
     * Format duration from seconds to mm:ss
     * @returns {string} Formatted duration
     */
    formatDuration() {
        const minutes = Math.floor(this.duration / 60);
        const seconds = Math.floor(this.duration % 60);
        return `${minutes}:${seconds < 10 ? '0' : ''}${seconds}`;
    }

    /**
     * Get a shortened preview of the index
     * @param {number} maxLength - Maximum length
     * @returns {string} Preview string
     */
    getIndexPreview(maxLength = 16) {
        if (this.index.length <= maxLength) {
            return this.index;
        }
        const start = this.index.substring(0, maxLength / 2);
        const end = this.index.substring(this.index.length - maxLength / 2);
        return `${start}...${end}`;
    }

    /**
     * Check if this track has cover art
     * @returns {boolean} True if cover art exists
     */
    hasCover() {
        return this.coverSvg !== null && this.coverSvg.length > 0;
    }

    /**
     * Get the full hierarchical path
     * @returns {string} Path string
     */
    getFullPath() {
        return `${this.roomId} / Wall ${this.wallNumber} / Shelf ${this.shelfNumber} / Track ${this.trackNumber}`;
    }

    toJSON() {
        return {
            index: this.index,
            position: this.position,
            metadata: this.metadata,
            duration: this.duration,
            durationFormatted: this.formatDuration(),
            coverSvg: this.coverSvg,
            address: this.address,
            fullPath: this.getFullPath()
        };
    }
}

export default Track;