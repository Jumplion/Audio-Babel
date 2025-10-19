class Room {
    constructor(genre) {
        this.genre = genre;
        this.walls = []; // Array to hold walls (artists)
    }

    addWall(artist) {
        const wall = new Wall(artist);
        this.walls.push(wall);
    }

    getWalls() {
        return this.walls;
    }

    toJSON() {
        return {
            genre: this.genre,
            walls: this.walls.map(wall => wall.toJSON())
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