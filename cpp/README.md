# Audio Babel - Hierarchical Audio Indexing System

A C++ implementation of a hierarchical audio indexing system inspired by Jorge Luis Borges' "The Library of Babel", but for audio instead of text.

## Overview

Audio Babel provides a way to:

- **Browse** through an infinite space of audio using a hierarchical Genre → Artist → Album → Track structure
- **Search** for audio clips that contain or are similar to a given audio sample
- **Generate** deterministic audio from hierarchical identifiers
- **Navigate** sequentially through the audio space

## Features

### Hierarchical Browsing

Just like the Library of Babel's hexagon/wall/shelf/volume system, Audio Babel uses:

- **Genre**: Top-level category (like hexagon names)
- **Artist**: Artists within a genre (like walls)
- **Album**: Albums by an artist (like shelves)
- **Track**: Individual tracks on an album (like volumes)

### Audio Search

- Find audio clips that **contain** a given audio sample
- Find audio clips that are **perceptually similar** to a reference
- Search within specific genres or artists
- Advanced spectral feature matching

### Audio Generation

- Generate deterministic audio from any valid hierarchical path
- Create random audio tracks
- Navigate sequentially through the audio space

## Dependencies

- **GMP** (GNU Multiple Precision Arithmetic Library) - for handling very large integers
- **FFTW3** - for fast Fourier transforms in audio processing
- **C++17** compatible compiler

### Installing Dependencies

#### Ubuntu/Debian

```bash
sudo apt-get install libgmp-dev libfftw3-dev
```

#### Windows (MSYS2/MinGW)

```bash
pacman -S mingw-w64-x86_64-gmp mingw-w64-x86_64-fftw
```

#### macOS (Homebrew)

```bash
brew install gmp fftw
```

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

### Basic Example

```cpp
#include "AudioBabel/AudioBrowser.h"
#include "AudioBabel/AudioIndex.h"

using namespace AudioBabel;

int main() {
    AudioBrowser browser;
    
    // Browse genres
    auto genres = browser.listGenres(0, 10);
    std::cout << "Available genres: " << std::endl;
    for (const auto& genre : genres) {
        std::cout << "  " << genre << std::endl;
    }
    
    // Generate a random track
    AudioIndex randomTrack = browser.getRandomTrack();
    std::cout << "Random track: " << randomTrack.getFullPath() << std::endl;
    
    // Generate audio samples
    std::vector<int32_t> samples = randomTrack.toAudioSamples();
    std::cout << "Generated " << samples.size() << " audio samples" << std::endl;
    
    return 0;
}
```

### Audio Search Example

```cpp
#include "AudioBabel/AudioSearch.h"
#include "AudioBabel/AudioIndex.h"

using namespace AudioBabel;

int main() {
    AudioSearch search;
    
    // Create some audio (e.g., load from file or generate)
    std::vector<int32_t> querySamples = loadAudioFromFile("query.wav");
    
    // Build search index from directory of audio files
    search.buildSearchIndex("/path/to/audio/index/files");
    
    // Find audio that contains the query
    auto results = search.findContainingAudio(querySamples, 48000, 10, 0.7);
    
    std::cout << "Found " << results.size() << " matching audio clips:" << std::endl;
    for (const auto& result : results) {
        std::cout << "  " << result.index.getFullPath() 
                  << " (similarity: " << result.similarityScore << ")" << std::endl;
    }
    
    return 0;
}
```

## Architecture

### Core Classes

- **AudioIndex**: Represents a single audio track with hierarchical identifiers
- **AudioFingerprint**: Perceptual audio representation for search and reconstruction
- **AudioSearch**: Search engine for finding similar or containing audio
- **AudioBrowser**: Navigation system for browsing the hierarchical space

### Design Principles

1. **Deterministic**: Given the same hierarchical path, always generate the same audio
2. **Infinite**: The space of possible audio is effectively unlimited
3. **Searchable**: Efficient lookup of audio by perceptual similarity
4. **Browsable**: Intuitive navigation through the audio space

## File Format

Audio indices can be serialized to `.aidx` files for persistent storage and fast loading.

## Testing

Run the test suite:

```bash
cd build
make test
```

## Examples

The `examples/` directory contains:

- **main.cpp**: Complete demonstration of all features
- Additional example programs showing specific use cases

## Comparison to Library of Babel

| Library of Babel | Audio Babel |
|------------------|-------------|
| Hexagon | Genre |
| Wall (1-4) | Artist |
| Shelf (1-5) | Album |
| Volume (1-32) | Track |
| Pages | Audio samples |
| Text search | Audio search |
| Random page | Random track |

## License

This project is open source. See LICENSE file for details.

## Contributing

Contributions are welcome! Please see CONTRIBUTING.md for guidelines.

## Future Enhancements

- Web interface for browsing
- Audio format support (WAV, MP3, etc.)
- Distributed search across multiple nodes
- Machine learning for improved similarity matching
- Real-time audio generation and streaming
