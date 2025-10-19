class Track {
    constructor(id, title, artist, album, genre, duration, filePath) {
        this.id = id; // Unique identifier for the track
        this.title = title; // Title of the track
        this.artist = artist; // Artist of the track
        this.album = album; // Album the track belongs to
        this.genre = genre; // Genre of the track
        this.duration = duration; // Duration of the track in seconds
        this.filePath = filePath; // Path to the audio file
    }

    // Method to get a formatted string of track details
    getTrackDetails() {
        return `${this.title} by ${this.artist} from the album ${this.album} [${this.genre}] - Duration: ${this.formatDuration()}`;
    }

    // Helper method to format duration from seconds to mm:ss
    formatDuration() {
        const minutes = Math.floor(this.duration / 60);
        const seconds = this.duration % 60;
        return `${minutes}:${seconds < 10 ? '0' : ''}${seconds}`;
    }
}

export default Track;