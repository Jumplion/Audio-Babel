# Audio Babel Record Store

A web-based navigation system for the Library of Babel audio indexing system, structured like a record store.

## Overview

The Audio Babel Record Store maps audio indexes to a hierarchical structure inspired by Jorge Luis Borges' Library of Babel:

- **Rooms** (Hexagons) represent **Genres**
- **Walls** (4 per room) represent **Artists**
- **Shelves** (20 per wall, like record store longboxes) represent **Albums**
- **Tracks** (32 per shelf, like records) represent individual audio files

Each room can hold **2,560 tracks** (4 × 20 × 32).

## Audio Constraints

To ensure manageable index sizes and consistent behavior:

- **Maximum Duration**: 2 minutes (120 seconds)
- **Format**: WAV (PCM)
- **Sample Rate**: 44,100 Hz (CD quality)
- **Bit Depth**: 16-bit
- **Channels**: Mono

These constraints ensure manageable index sizes (~14 MB base64 encoded).

For detailed documentation on the index architecture, see [`INDEX_ARCHITECTURE.md`](./INDEX_ARCHITECTURE.md).

## Project Structure

The project is organized into the following directories and files:

- **public/**: Contains all the static files served to the client.
  - **index.html**: Main entry point of the website.
  - **browse.html**: Page to browse through different rooms (genres) and their corresponding walls (artists).
  - **room.html**: Displays details of a specific room (genre), including its walls (artists) and shelves (albums).
  - **wall.html**: Shows details of a specific wall (artist) within a room, including its shelves (albums).
  - **shelf.html**: Presents the albums contained in a specific shelf (longbox) of a wall (artist).
  - **track.html**: Provides details about a specific track, including playback options.
  - **css/**: Contains stylesheets for the website.
  - **js/**: Contains JavaScript files for application logic.
  - **assets/**: Contains SVG assets used in the website.

- **src/**: Contains server-side code and logic.
  - **server.js**: Sets up the Node.js server.
  - **routes/**: Contains route definitions for the application.
  - **services/**: Contains services for parsing, metadata extraction, and cover generation.
  - **models/**: Defines data models for Rooms, Walls, Shelves, and Tracks.
  - **utils/**: Contains utility functions for encoding and layout management.

- **components/**: Contains reusable HTML components for the application.

- **package.json**: Configuration file for npm, including dependencies and scripts.

- **README.md**: This documentation file.

## Setup Instructions

1. **Clone the repository**:
   ```
   git clone <repository-url>
   cd audio-babel-record-store
   ```

2. **Install dependencies**:
   ```
   npm install
   ```

3. **Run the server**:
   ```
   node src/server.js
   ```

4. **Access the application**:
   Open your web browser and navigate to `http://localhost:3000` (or the port specified in your server configuration).

## Usage Guidelines

- Navigate through the record store using the navigation bar to explore different genres, artists, albums, and tracks.
- Use the search functionality to quickly find specific tracks or albums.
- The application is designed to be responsive, ensuring a seamless experience across devices.

## Unique Indexing System

The project implements a unique indexing system that combines genre, artist, album, and track into a single identifier. This ensures that each index is unique within its context, facilitating efficient data retrieval and management.

## Visual Design

The application utilizes SVG graphics for the hexagonal layout, ensuring scalability and maintaining a visually appealing design that reflects the record store theme.

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests to enhance the functionality and design of the Audio Babel Record Store.

## License

This project is licensed under the MIT License. See the LICENSE file for more details.