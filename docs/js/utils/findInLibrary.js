/**
 * Hands a library position (room/wall/shelf/album/track) from the Search
 * page to the Browse page via sessionStorage, so Browse can auto-navigate to
 * it on load. sessionStorage (not a URL param) keeps the payload off the
 * address bar and self-clears once Browse consumes it.
 */

const STORAGE_KEY = 'audioBabel.findInLibraryTarget';

export function setFindInLibraryTarget(position) {
  sessionStorage.setItem(STORAGE_KEY, JSON.stringify(position));
}

// Reads and clears the pending library position, if any.
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
