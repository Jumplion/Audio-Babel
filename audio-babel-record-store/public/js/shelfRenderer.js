// shelfRenderer.js

function renderShelf(shelfData) {
    const shelfContainer = document.getElementById('shelf-container');
    shelfContainer.innerHTML = ''; // Clear previous content

    const shelfTitle = document.createElement('h2');
    shelfTitle.textContent = shelfData.title;
    shelfContainer.appendChild(shelfTitle);

    const trackList = document.createElement('ul');
    shelfData.tracks.forEach(track => {
        const trackItem = document.createElement('li');
        trackItem.textContent = `${track.title} by ${track.artist}`;
        trackItem.onclick = () => playTrack(track.id);
        trackList.appendChild(trackItem);
    });

    shelfContainer.appendChild(trackList);
}

function playTrack(trackId) {
    // Logic to play the selected track
    console.log(`Playing track with ID: ${trackId}`);
}

// Example usage
// Assuming shelfData is fetched from the server or defined elsewhere
// renderShelf(shelfData);