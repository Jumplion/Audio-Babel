class Wall {
    constructor(artist, shelves = []) {
        this.artist = artist; // The name of the artist
        this.shelves = shelves; // An array of Shelf objects
    }

    addShelf(shelf) {
        this.shelves.push(shelf); // Adds a Shelf object to the wall
    }

    getShelf(album) {
        return this.shelves.find(shelf => shelf.album === album); // Retrieves a shelf by album name
    }

    toJSON() {
        return {
            artist: this.artist,
            shelves: this.shelves.map(shelf => shelf.toJSON()) // Converts shelves to JSON format
        };
    }
}

export default Wall;