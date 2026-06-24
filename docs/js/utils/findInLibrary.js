/**
 * findInLibrary.js
 *
 * Hands a library position (room/wall/shelf/album/track) from the Search
 * page to the Browse page via sessionStorage, so Browse can auto-navigate
 * to it on load. sessionStorage (not a URL param) keeps the payload off
 * the address bar and self-clears once Browse consumes it.
 */

const STORAGE_KEY = 'audioBabel.findInLibraryTarget';

/**
 * Store a library position for the Browse page to pick up on next load.
 * @param {{room: string, wall: number, shelf: number, album: number, track: number}} position
 */
export function setFindInLibraryTarget(position) {
  sessionStorage.setItem(STORAGE_KEY, JSON.stringify(position));
}

/**
 * Read and clear the pending library position, if any.
 * @returns {{room: string, wall: number, shelf: number, album: number, track: number}|null}
 */
export function consumeFindInLibraryTarget() {
  const raw = sessionStorage.getItem(STORAGE_KEY);
  if (!raw) return null;

  sessionStorage.removeItem(STORAGE_KEY);
  try {
    return JSON.parse(raw);
  } catch (e) {
    return null;
  }
}
