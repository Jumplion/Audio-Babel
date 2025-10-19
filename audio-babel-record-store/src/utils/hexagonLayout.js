// src/utils/hexagonLayout.js

function createHexagon(x, y, size) {
    const points = [];
    for (let i = 0; i < 6; i++) {
        const angle = (Math.PI / 3) * i;
        const xOffset = size * Math.cos(angle);
        const yOffset = size * Math.sin(angle);
        points.push(`${x + xOffset},${y + yOffset}`);
    }
    return points.join(' ');
}

function drawHexagon(svg, x, y, size, color) {
    const hexagon = document.createElementNS('http://www.w3.org/2000/svg', 'polygon');
    hexagon.setAttribute('points', createHexagon(x, y, size));
    hexagon.setAttribute('fill', color);
    svg.appendChild(hexagon);
}

function renderRoomHexagons(svg, rooms) {
    const hexagonSize = 50; // Adjust size as needed
    const spacing = 10; // Space between hexagons
    let xOffset = 0;
    let yOffset = 0;

    rooms.forEach((room, index) => {
        drawHexagon(svg, xOffset, yOffset, hexagonSize, room.color);
        xOffset += hexagonSize * 1.5 + spacing; // Move to the right for the next hexagon
        if ((index + 1) % 5 === 0) { // Move to the next row after 5 hexagons
            xOffset = 0;
            yOffset += (Math.sqrt(3) * hexagonSize) + spacing; // Adjust for hexagon height
        }
    });
}

export { createHexagon, drawHexagon, renderRoomHexagons };