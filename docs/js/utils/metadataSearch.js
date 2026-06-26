/**
 * metadataSearch.js
 *
 * Cross-room search by genre/artist/album/track name, combining one or more
 * fields into a single set of matches. The underlying WASM `findByName` (see
 * IndexFinder.h) only checks one hierarchy level at a time — it has no notion
 * of "artist AND album". This module gets an AND across levels for free,
 * without any new C++/WASM surface: it drives the scan from the deepest
 * filled-in field (the most selective single-level search), then verifies
 * any shallower filled-in fields against that candidate's *actual* cosmetic
 * names via the existing getGenreNames/getArtistNames/getAlbumNames batch
 * accessors (the same ones the Browse page already uses).
 */

import { filterToBase64UrlChars } from './validationUtils.js';

// Mirrors IndexNaming.cpp's private DECORATION_FACTOR. Duplicated here for
// the same reason validationUtils.js duplicates isValidBase64Url: JS needs
// to know a name's exact display width to give useful UI hints (placeholder/
// maxlength), and that width isn't itself exposed over the WASM boundary.
const DECORATION_FACTOR = 4096;

const LEVELS = ['genre', 'artist', 'album', 'track']; // shallow -> deep

function digitsCovering(domain) {
  let width = 0;
  let cap = 1;
  while (cap < domain) {
    cap *= 64;
    width += 1;
  }
  return width;
}

/**
 * Compute the fixed display width (in base64 characters) of a name at each
 * hierarchy level, given the room's library constants. Mirrors IndexNaming's
 * digitsCovering(realCount * DECORATION_FACTOR).
 * @param {{wallsPerRoom: number, shelvesPerWall: number, albumsPerShelf: number, tracksPerAlbum: number}} constants
 * @returns {{genre: number, artist: number, album: number, track: number}}
 */
export function computeNameWidths(constants) {
  const { wallsPerRoom, shelvesPerWall, albumsPerShelf, tracksPerAlbum } = constants;
  const genreCount = wallsPerRoom;
  const artistCount = wallsPerRoom * shelvesPerWall;
  const albumCount = wallsPerRoom * shelvesPerWall * albumsPerShelf;
  const trackCount = wallsPerRoom * shelvesPerWall * albumsPerShelf * tracksPerAlbum;

  return {
    genre: digitsCovering(genreCount * DECORATION_FACTOR),
    artist: digitsCovering(artistCount * DECORATION_FACTOR),
    album: digitsCovering(albumCount * DECORATION_FACTOR),
    track: digitsCovering(trackCount * DECORATION_FACTOR)
  };
}

/**
 * Filter a field value down to valid, width-capped characters as the user types.
 * @param {string} value - Raw input value
 * @param {number} width - Fixed display width for this field's level
 * @returns {string} Sanitized value, at most `width` characters
 */
export function sanitizeMetadataFieldValue(value, width) {
  return filterToBase64UrlChars(value || '').slice(0, width);
}

/**
 * Resolve the cosmetic name a candidate match actually has at a given
 * shallower level, using the same batch accessors the Browse page uses.
 * @param {Object} wasm - Initialized IndexWasm instance
 * @param {{room: string, wall: number, shelf: number, album: number}} candidate
 * @param {'genre'|'artist'|'album'} level
 * @returns {string|undefined}
 */
function resolveNameForLevel(wasm, candidate, level) {
  switch (level) {
    case 'genre':
      return JSON.parse(wasm.module.getGenreNames(candidate.room))[candidate.wall];
    case 'artist':
      return JSON.parse(wasm.module.getArtistNames(candidate.room, candidate.wall))[candidate.shelf];
    case 'album':
      return JSON.parse(wasm.module.getAlbumNames(candidate.room, candidate.wall, candidate.shelf))[candidate.album];
    default:
      return undefined;
  }
}

function matchesShallowerFields(wasm, candidate, shallowerLevels, fields) {
  return shallowerLevels.every((level) => resolveNameForLevel(wasm, candidate, level) === fields[level]);
}

/**
 * Search the library by any combination of genre/artist/album/track names.
 * @param {Object} wasm - Initialized IndexWasm instance
 * @param {{genre?: string, artist?: string, album?: string, track?: string}} fields - Exact names to match, per level
 * @param {Object} [searchOptions]
 * @param {number} [searchOptions.maxResults=10] - Matches to return
 * @param {number} [searchOptions.maxRoomsToScan=2000000] - Per-attempt room-scan budget (passed through to findByName)
 * @returns {Promise<Array<Object>>} Up to maxResults IndexMatch-shaped objects satisfying every filled-in field
 */
export async function searchByMetadata(wasm, fields, searchOptions = {}) {
  const { maxResults = 10, maxRoomsToScan = 2_000_000 } = searchOptions;

  const filledLevels = LEVELS.filter((level) => fields[level] && fields[level].length > 0);
  if (filledLevels.length === 0) return [];

  const deepestLevel = filledLevels[filledLevels.length - 1];
  const shallowerLevels = filledLevels.slice(0, -1);

  let candidateBudget = maxResults;
  let matches = [];

  // Shallower-field verification can reject candidates findByName already
  // considered a match (e.g. the right album, wrong artist). Widen the
  // single-level candidate pool a few times before giving up, rather than
  // settling for fewer results than the caller asked for.
  for (let attempt = 0; attempt < 5; attempt += 1) {
    const json = wasm.module.findByName(fields[deepestLevel], deepestLevel, candidateBudget, maxRoomsToScan);
    const candidates = JSON.parse(json);
    if (candidates && candidates.error) {
      throw new Error(candidates.error);
    }

    matches = candidates.filter((candidate) => matchesShallowerFields(wasm, candidate, shallowerLevels, fields)).slice(0, maxResults);

    const scanExhausted = candidates.length < candidateBudget;
    if (matches.length >= maxResults || scanExhausted || shallowerLevels.length === 0) {
      break;
    }
    candidateBudget *= 4;
  }

  return matches;
}
