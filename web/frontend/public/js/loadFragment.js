// loadFragment(containerSelector, fragmentPath)
// Fetches an HTML fragment and injects it into the container element.
// Returns an object with a `get` helper to resolve elements inside the injected fragment.
export async function loadFragment(containerSelector, fragmentPath) {
  const container = document.querySelector(containerSelector);
  if (!container) throw new Error('Container not found: ' + containerSelector);

  const resp = await fetch(fragmentPath);
  if (!resp.ok) throw new Error('Failed to fetch fragment: ' + fragmentPath + ' (' + resp.status + ')');
  const html = await resp.text();
  container.innerHTML = html;

  return {
    // get(selector) runs querySelector within the container
    get(selector) {
      return container.querySelector(selector);
    },
    // getAll(selector) runs querySelectorAll within the container
    getAll(selector) {
      return Array.from(container.querySelectorAll(selector));
    },
    // expose the container for further DOM manipulation
    container,
  };
}
