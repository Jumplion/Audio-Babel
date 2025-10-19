/**
 * Loads an HTML fragment from a URL and injects it into a container element.
 * Handles inline and external scripts within the fragment.
 * @param {string} containerSelector - CSS selector for the target container
 * @param {string} fragmentPath - Path to the HTML fragment file
 * @returns {Promise<Object>} Fragment helper with get()/getAll() methods and container reference
 */
export async function loadFragment(containerSelector, fragmentPath) {
  const container = document.querySelector(containerSelector);
  if (!container) throw new Error("Container not found: " + containerSelector);

  const resp = await fetch(fragmentPath);
  if (!resp.ok)
    throw new Error(
      "Failed to fetch fragment: " + fragmentPath + " (" + resp.status + ")"
    );
  const html = await resp.text();
  // Inject the fragment and execute any <script> tags contained within it.
  const tmp = document.createElement('div');
  tmp.innerHTML = html;

  // Clear the container and append non-script nodes
  container.innerHTML = '';
  const scripts = [];
  Array.from(tmp.childNodes).forEach((node) => {
    if (node.nodeType === Node.ELEMENT_NODE && node.tagName.toLowerCase() === 'script') {
      scripts.push(node);
    } else {
      container.appendChild(node.cloneNode(true));
    }
  });

  // Execute scripts serially. For external scripts (src) we wait for load.
  for (const s of scripts) {
    const newScript = document.createElement('script');
    if (s.type) newScript.type = s.type;
    if (s.src) {
      newScript.src = s.src;
      // preserve module behavior if present
      if (s.type) newScript.type = s.type;
      newScript.async = false;
      container.appendChild(newScript);
      await new Promise((resolve, reject) => {
        newScript.onload = resolve;
        newScript.onerror = () => reject(new Error('Failed to load script ' + s.src));
      });
    } else {
      // inline script: copy text and append to execute
      newScript.textContent = s.textContent;
      if (s.type) newScript.type = s.type;
      container.appendChild(newScript);
      // inline scripts execute immediately on append
    }
  }

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
