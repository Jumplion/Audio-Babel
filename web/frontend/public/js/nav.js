// nav.js
// Exports highlightActiveNav(containerSelector) which finds the nav links inside
// the injected navbar and sets the active class based on current location.
export function highlightActiveNav(containerSelector = '#navContainer') {
  try {
    const container = document.querySelector(containerSelector);
    if (!container) return;
    const links = container.querySelectorAll('.nav-links a');
    if (!links || links.length === 0) return;
    const cur = location.pathname.split('/').pop() || 'index.html';
    links.forEach((a) => {
      const href = a.getAttribute('href');
      // compare trailing part of href to current path
      const name = href.split('/').pop();
      if (name === cur) {
        a.classList.add('active');
        a.setAttribute('aria-current', 'page');
      } else {
        a.classList.remove('active');
        a.removeAttribute('aria-current');
      }
    });
  } catch (e) {
    // non-fatal
    console.warn('highlightActiveNav failed', e);
  }
}
