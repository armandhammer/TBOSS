// SPDX-License-Identifier: GPL-3.0-or-later - the TBOSS project page and manual: light/dark theme, the
// manual's contents (where you are, a filter, a drawer on narrow screens), back to top, and the
// released version on the project page (firmware/tboss.json, published with the installer).
(() => {
  const root = document.documentElement;
  const store = { get(k) { try { return localStorage.getItem(k); } catch { return null; } },
    set(k, v) { try { localStorage.setItem(k, v); } catch { /* private window: not remembered */ } } };

  // Theme: the system's, until the button picks one (remembered on this computer).
  const themeButton = document.querySelector('[data-theme-toggle]');
  const dark = () => (root.dataset.theme || (matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light')) === 'dark';
  const label = () => themeButton && themeButton.setAttribute('aria-label', dark() ? 'Light theme' : 'Dark theme');
  if (themeButton) {
    label();
    themeButton.addEventListener('click', () => { root.dataset.theme = dark() ? 'light' : 'dark'; store.set('tboss-theme', root.dataset.theme); label(); });
  }

  // The released version on the project page.
  const version = document.getElementById('version');
  if (version) {
    fetch('firmware/tboss.json', { cache: 'no-store' }).then((r) => (r.ok ? r.json() : null)).then((m) => {
      if (m && m.version) { version.textContent = m.version; version.closest('[hidden]')?.removeAttribute('hidden'); }
    }).catch(() => {});
  }

  // Back to top.
  const toTop = document.querySelector('.to-top');
  if (toTop) addEventListener('scroll', () => toTop.classList.toggle('shown', scrollY > 1200), { passive: true });

  const toc = document.querySelector('.toc');
  if (!toc) return;
  const sidebar = document.querySelector('.sidebar');
  const drawer = document.querySelector('[data-contents]');
  const close = () => { sidebar.classList.remove('open'); drawer?.setAttribute('aria-expanded', 'false'); };
  drawer?.addEventListener('click', () => {
    const open = sidebar.classList.toggle('open');
    drawer.setAttribute('aria-expanded', String(open));
    if (open) toc.querySelector('a.active')?.scrollIntoView({ block: 'center' });
  });
  toc.addEventListener('click', (e) => { if (e.target.closest('a')) close(); });
  document.addEventListener('keydown', (e) => { if (e.key === 'Escape') close(); });

  // Where you are: the last heading above the top of the window lights in the contents, and its
  // chapter opens.
  const links = new Map([...toc.querySelectorAll('a')].map((a) => [decodeURIComponent(a.hash.slice(1)), a]));
  const headings = [...document.querySelectorAll('.manual h2[id], .manual h3[id]')].filter((h) => links.has(h.id));
  let current = null;
  const mark = () => {
    const line = innerHeight * 0.25;
    let at = headings[0];
    for (const h of headings) { if (h.getBoundingClientRect().top <= line) at = h; else break; }
    if (!at || at === current) return;
    current = at;
    toc.querySelectorAll('a.active').forEach((a) => a.classList.remove('active'));
    toc.querySelectorAll('li.open').forEach((li) => li.classList.remove('open'));
    const a = links.get(at.id);
    a.classList.add('active');
    let li = a.closest('li');
    while (li) { li.classList.add('open'); li = li.parentElement.closest('li'); }
    if (!sidebar.classList.contains('open')) {
      const r = a.getBoundingClientRect(), s = sidebar.getBoundingClientRect();
      if (r.top < s.top + 40 || r.bottom > s.bottom - 40) sidebar.scrollTop += r.top - s.top - s.height / 3;
    }
  };
  let queued = false;
  addEventListener('scroll', () => { if (!queued) { queued = true; requestAnimationFrame(() => { queued = false; mark(); }); } }, { passive: true });
  addEventListener('hashchange', mark);
  mark();

  // Filter the contents by words in the headings.
  const filter = document.querySelector('.toc-filter');
  filter?.addEventListener('input', () => {
    const words = filter.value.trim().toLowerCase().split(/\s+/).filter(Boolean);
    toc.classList.toggle('filtering', words.length > 0);
    for (const li of toc.querySelectorAll('li')) {
      const text = li.textContent.toLowerCase();
      li.classList.toggle('hidden', words.length > 0 && !words.every((w) => text.includes(w)));
    }
  });
})();
