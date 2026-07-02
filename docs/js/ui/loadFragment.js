// Returns a helper scoped to the injected fragment: get()/getAll() run
// querySelector(All) within the fragment's container.
export async function loadFragment(containerSelector, fragmentPath) {
  const container = document.querySelector(containerSelector);
  if (!container) throw new Error('Container not found: ' + containerSelector);

  const resp = await fetch(fragmentPath);
  if (!resp.ok)
    throw new Error('Failed to fetch fragment: ' + fragmentPath + ' (' + resp.status + ')');
  const html = await resp.text();
  const tmp = document.createElement('div');
  tmp.innerHTML = html;

  container.innerHTML = '';
  const scripts = [];
  Array.from(tmp.childNodes).forEach((node) => {
    if (node.nodeType === Node.ELEMENT_NODE && node.tagName.toLowerCase() === 'script') {
      scripts.push(node);
    } else {
      container.appendChild(node.cloneNode(true));
    }
  });

  // Run scripts serially and in original order; external scripts (src) are
  // awaited via onload before the next one runs.
  for (const s of scripts) {
    const newScript = document.createElement('script');
    if (s.type) newScript.type = s.type;
    if (s.src) {
      newScript.src = s.src;
      newScript.async = false;
      container.appendChild(newScript);
      await new Promise((resolve, reject) => {
        newScript.onload = resolve;
        newScript.onerror = () => reject(new Error('Failed to load script ' + s.src));
      });
    } else {
      newScript.textContent = s.textContent;
      if (s.type) newScript.type = s.type;
      container.appendChild(newScript); // inline scripts execute immediately on append
    }
  }

  return {
    get(selector) {
      return container.querySelector(selector);
    },
    getAll(selector) {
      return Array.from(container.querySelectorAll(selector));
    },
    container,
  };
}
