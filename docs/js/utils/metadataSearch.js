/**
 * metadataSearch.js
 *
 * Find indexes by any combination of genre/artist/album/track names.
 *
 * There is no search any more: because the naming permutation is invertible
 * (see IndexNaming.h), the WASM `constructByNames` turns the requested names
 * straight into concrete indexes that carry them. Pinned fields are fixed;
 * unfilled fields and each index's high "discriminator" bits are randomized, so
 * every result is a distinct candidate — different "room", same metadata.
 */

import { filterToBase64UrlChars } from './validationUtils.js';

const LEVELS = ['genre', 'artist', 'album', 'track'];

/**
 * The fixed maximum display width (in base64 characters) of each field name.
 * Every level shares the same cap (IndexNaming::NAME_MAX_CHARS), surfaced via
 * getLibraryConstants so JS never hardcodes it.
 * @param {{nameMaxChars: number}} constants
 * @returns {{genre: number, artist: number, album: number, track: number}}
 */
export function computeNameWidths(constants) {
  const width = constants.nameMaxChars;
  return { genre: width, artist: width, album: width, track: width };
}

/**
 * Filter a field value down to valid, width-capped characters as the user types.
 * @param {string} value - Raw input value
 * @param {number} width - Maximum name width for this level
 * @returns {string} Sanitized value, at most `width` characters
 */
export function sanitizeMetadataFieldValue(value, width) {
  return filterToBase64UrlChars(value || '').slice(0, width);
}

/**
 * Build indexes matching any combination of genre/artist/album/track names.
 * @param {Object} wasm - Initialized IndexWasm instance
 * @param {{genre?: string, artist?: string, album?: string, track?: string}} fields - Names to pin, per level
 * @param {Object} [options]
 * @param {number} [options.maxResults=10] - How many candidate indexes to return
 * @param {number} [options.seed] - Randomness seed (defaults to a fresh random value each call)
 * @returns {Promise<Array<{indexBase64: string, position: Object, names: Object}>>}
 */
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

  const results = JSON.parse(json);
  if (results && results.error) {
    throw new Error(results.error);
  }

  return results.map((r) => ({
    indexBase64: r.indexBase64,
    position: { room: r.room, wall: r.wall, shelf: r.shelf, album: r.album, track: r.track },
    names: { genre: r.genreName, artist: r.artistName, album: r.albumName, track: r.trackName }
  }));
}
