/**
 * Find indexes by any combination of genre/artist/album/track names.
 *
 * Because the naming permutation is invertible (see IndexNaming.h), the WASM
 * `constructByNames` turns the requested names straight into concrete
 * indexes that carry them — not a search. Pinned fields are fixed; unfilled
 * fields and each index's high "discriminator" bits are randomized, so every
 * result is a distinct candidate — different "room", same metadata.
 */

import { filterToBase64UrlChars } from './validationUtils.js';

const LEVELS = ['genre', 'artist', 'album', 'track'];

// Every level shares the same display width cap (IndexNaming::NAME_MAX_CHARS),
// surfaced via getLibraryConstants so JS never hardcodes it.
export function computeNameWidths(constants) {
  const width = constants.nameMaxChars;
  return { genre: width, artist: width, album: width, track: width };
}

export function sanitizeMetadataFieldValue(value, width) {
  return filterToBase64UrlChars(value || '').slice(0, width);
}

function parseConstructedResults(json) {
  const results = JSON.parse(json);
  if (results && results.error) {
    throw new Error(results.error);
  }

  return results.map((r) => ({
    indexBase64: r.indexBase64,
    position: { room: r.room, wall: r.wall, shelf: r.shelf, album: r.album, track: r.track },
    names: { genre: r.genreName, artist: r.artistName, album: r.albumName, track: r.trackName },
  }));
}

// Builds indexes matching any combination of genre/artist/album/track names.
// fields: names to pin per level; options.maxResults defaults to 10,
// options.seed defaults to a fresh random value each call.
export async function searchByMetadata(wasm, fields, options = {}) {
  const { maxResults = 10, seed } = options;

  const anyFilled = LEVELS.some((level) => fields[level] && fields[level].length > 0);
  if (!anyFilled) return [];

  const actualSeed = seed ?? Math.floor(Math.random() * 0x1_0000_0000);

  const json = wasm.module.constructByNames(
    fields.genre || '',
    fields.artist || '',
    fields.album || '',
    fields.track || '',
    maxResults,
    actualSeed
  );

  return parseConstructedResults(json);
}

// Builds indexes whose cover art renders the given pixel grid. Like
// searchByMetadata, this constructs rather than scans: the cover
// byte-to-pixel mapping is invertible (see IndexMetadata.h), so the target
// pixels are turned straight into indexes that carry them. Names, position,
// and audio vary per candidate; only the cover is pinned. `pixels` must be
// packed 8-bit RGB in reading order, exactly coverPixelBytes long (the shape
// quantizeImageToCoverPixels produces).
export async function searchByCover(wasm, pixels, options = {}) {
  const { maxResults = 10, seed } = options;

  const actualSeed = seed ?? Math.floor(Math.random() * 0x1_0000_0000);

  const json = wasm.module.constructByCover(pixels, maxResults, actualSeed);

  return parseConstructedResults(json);
}
