class Shelf {
    constructor(artist, album) {
        this.artist = artist; // The artist of the album
        this.album = album;   // The name of the album
        this.tracks = [];     // Array to hold tracks in the album
    }

    addTrack(track) {
        this.tracks.push(track); // Add a track to the shelf
    }

    getTrack(trackIndex) {
        return this.tracks[trackIndex]; // Retrieve a track by its index
    }

    getTrackCount() {
        return this.tracks.length; // Get the total number of tracks in the shelf
    }

    toJSON() {
        return {
            artist: this.artist,
            album: this.album,
            tracks: this.tracks.map(track => track.toJSON()) // Convert tracks to JSON
        };
    }
}

export default Shelf;