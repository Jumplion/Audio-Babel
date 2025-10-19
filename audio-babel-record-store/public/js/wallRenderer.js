// wallRenderer.js

document.addEventListener('DOMContentLoaded', function() {
    const wallContainer = document.getElementById('wall-container');
    const artistId = wallContainer.dataset.artistId; // Assuming the artist ID is stored in a data attribute

    fetch(`/api/walls/${artistId}`)
        .then(response => response.json())
        .then(data => {
            renderWall(data);
        })
        .catch(error => {
            console.error('Error fetching wall data:', error);
        });
});

function renderWall(wallData) {
    const wallContainer = document.getElementById('wall-container');
    wallContainer.innerHTML = '';

    const wallTitle = document.createElement('h2');
    wallTitle.textContent = wallData.artistName;
    wallContainer.appendChild(wallTitle);

    const shelvesList = document.createElement('ul');
    wallData.shelves.forEach(shelf => {
        const shelfItem = document.createElement('li');
        shelfItem.textContent = shelf.albumName;
        shelfItem.addEventListener('click', () => {
            window.location.href = `/shelves/${shelf.id}`;
        });
        shelvesList.appendChild(shelfItem);
    });

    wallContainer.appendChild(shelvesList);
}