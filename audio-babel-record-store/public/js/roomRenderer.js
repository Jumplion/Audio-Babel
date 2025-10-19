// This file handles rendering the room layout and its contents.

document.addEventListener("DOMContentLoaded", function() {
    const roomContainer = document.getElementById("room-container");
    const genre = roomContainer.dataset.genre; // Get genre from data attribute
    const walls = roomContainer.dataset.walls ? JSON.parse(roomContainer.dataset.walls) : []; // Parse walls data

    function renderWalls() {
        walls.forEach(wall => {
            const wallElement = document.createElement("div");
            wallElement.className = "wall";
            wallElement.innerHTML = `
                <h2>${wall.artist}</h2>
                <div class="shelves" data-shelves='${JSON.stringify(wall.shelves)}'></div>
            `;
            roomContainer.appendChild(wallElement);
            renderShelves(wallElement.querySelector(".shelves"), wall.shelves);
        });
    }

    function renderShelves(shelvesContainer, shelves) {
        shelves.forEach(shelf => {
            const shelfElement = document.createElement("div");
            shelfElement.className = "shelf";
            shelfElement.innerHTML = `
                <h3>${shelf.album}</h3>
                <div class="tracks" data-tracks='${JSON.stringify(shelf.tracks)}'></div>
            `;
            shelvesContainer.appendChild(shelfElement);
            renderTracks(shelfElement.querySelector(".tracks"), shelf.tracks);
        });
    }

    function renderTracks(tracksContainer, tracks) {
        tracks.forEach(track => {
            const trackElement = document.createElement("div");
            trackElement.className = "track";
            trackElement.innerHTML = `
                <p>${track.title}</p>
                <button onclick="playTrack('${track.id}')">Play</button>
            `;
            tracksContainer.appendChild(trackElement);
        });
    }

    function playTrack(trackId) {
        // Logic to play the track
        console.log(`Playing track with ID: ${trackId}`);
    }

    renderWalls();
});