// src/services/coverGenerator.js

const { createCanvas } = require('canvas');

function generateCover(title, artist, album, color) {
    const canvas = createCanvas(256, 256);
    const context = canvas.getContext('2d');

    // Fill background with the dominant color
    context.fillStyle = color;
    context.fillRect(0, 0, canvas.width, canvas.height);

    // Set text properties
    context.fillStyle = 'white';
    context.font = 'bold 20px Arial';
    context.textAlign = 'center';
    context.textBaseline = 'middle';

    // Draw text on the cover
    context.fillText(title, canvas.width / 2, canvas.height / 2 - 10);
    context.fillText(artist, canvas.width / 2, canvas.height / 2 + 10);
    context.fillText(album, canvas.width / 2, canvas.height / 2 + 30);

    return canvas.toDataURL(); // Return the cover image as a data URL
}

module.exports = {
    generateCover,
};