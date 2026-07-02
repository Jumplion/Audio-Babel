export function highlightActiveNav(containerSelector = '#navContainer') {
  try {
    const container = document.querySelector(containerSelector);
    if (!container) return;
    const links = container.querySelectorAll('.nav-links a');
    if (!links || links.length === 0) return;
    const cur = location.pathname.split('/').pop() || 'index.html';
    links.forEach((a) => {
      const href = a.getAttribute('href');
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
    console.warn('highlightActiveNav failed', e);
  }
}
