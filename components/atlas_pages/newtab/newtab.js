// SPDX-License-Identifier: BSD-3-Clause
// Atlas new tab page: the badge and the name, a search field, the pinned pages, and the pages
// to pick up. Keyboard first. No network, no markup written from text (the page runs under
// Trusted Types), no inline handlers. Everything the page knows comes from the host (host.js).
'use strict';

(function () {
  const host = window.atlasNewTab;
  const CATEGORIES = ['web', 'talk', 'files', 'media', 'store', 'system'];
  const MAX_PINNED = 12;
  const MAX_RECENT = 4;

  // Strings: the English defaults live in the page; the host may send its own (state.strings)
  // for the person's language, one entry per key, with {0} where a value goes.
  const strings = {
    opened: 'Opened {0}',
    openedJustNow: 'Opened just now',
    removeFromPinned: 'Remove {0} from Pinned',
    removedFromPinned: '{0} removed from Pinned',
    searchPlaceholder: 'Search or enter an address',
  };

  const page = document.getElementById('page');
  const pins = document.getElementById('pins');
  const pinsEmpty = document.getElementById('pins-empty');
  const recentSection = document.getElementById('recent');
  const recents = document.getElementById('recents');
  const recentEmpty = document.getElementById('recent-empty');
  const emptyAction = document.getElementById('empty-action');
  const status = document.getElementById('status');
  const live = document.getElementById('live');
  const form = document.getElementById('search');
  const query = document.getElementById('query');
  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)');

  function fmt(template, value) {
    return template.replace('{0}', () => value);
  }

  function reduceMotion() {
    return reduced.matches || document.documentElement.classList.contains('reduce-motion');
  }

  function applyStrings(sent) {
    if (sent && typeof sent === 'object') {
      Object.keys(strings).forEach((k) => {
        if (typeof sent[k] === 'string') strings[k] = sent[k];
      });
      document.querySelectorAll('[data-s]').forEach((el) => {
        const v = sent[el.dataset.s];
        if (typeof v === 'string') el.textContent = v;
      });
    }
    query.placeholder = strings.searchPlaceholder;
    query.setAttribute('aria-label', strings.searchPlaceholder);
  }

  // A glyph is a copy of the template's svg (the browser parses it into the svg namespace)
  // pointing at a symbol of the sprite.
  const glyphTemplate = document.getElementById('glyph-template');
  function svgUse(id, className) {
    const svg = glyphTemplate.content.firstElementChild.cloneNode(true);
    svg.setAttribute('class', className);
    svg.firstElementChild.setAttribute('href', '#' + id);
    return svg;
  }

  function element(tag, className, text) {
    const el = document.createElement(tag);
    if (className) el.className = className;
    if (text !== undefined) el.textContent = text;
    return el;
  }

  // Only web addresses get an href: the card is a real link (status text, middle click), but
  // the host decides what opens.
  function webUrl(value) {
    try {
      const u = new URL(String(value));
      return u.protocol === 'https:' || u.protocol === 'http:' ? u.href : null;
    } catch (e) {
      return null;
    }
  }

  function hostName(value) {
    try {
      return new URL(String(value)).hostname.replace(/^www\./, '');
    } catch (e) {
      return String(value);
    }
  }

  function titleOf(item) {
    const t = typeof item.title === 'string' ? item.title.trim() : '';
    return t || hostName(item.url);
  }

  function openLike(link, url) {
    link.addEventListener('click', (e) => {
      e.preventDefault();
      host.open(url, { newTab: e.ctrlKey || e.metaKey });
    });
    link.addEventListener('auxclick', (e) => {
      if (e.button !== 1) return;
      e.preventDefault();
      host.open(url, { newTab: true });
    });
  }

  // "Opened 20 minutes ago", "Opened yesterday", "Opened 3 days ago", then the date.
  function openedText(when) {
    const then = Number(when);
    if (!Number.isFinite(then)) return '';
    const lang = document.documentElement.lang || navigator.language || 'en';
    const minutes = Math.round((Date.now() - then) / 60000);
    if (minutes < 1) return strings.openedJustNow;
    const rtf = new Intl.RelativeTimeFormat(lang, { numeric: 'auto' });
    let ago;
    if (minutes < 60) {
      ago = rtf.format(-minutes, 'minute');
    } else if (minutes < 24 * 60) {
      ago = rtf.format(-Math.round(minutes / 60), 'hour');
    } else {
      const startOfDay = (t) => { const d = new Date(t); d.setHours(0, 0, 0, 0); return d.getTime(); };
      const days = Math.round((startOfDay(Date.now()) - startOfDay(then)) / 86400000);
      ago = days < 14
        ? rtf.format(-Math.max(days, 1), 'day')
        : new Intl.DateTimeFormat(lang, { day: 'numeric', month: 'short' }).format(then);
    }
    return fmt(strings.opened, ago);
  }

  function announce(text) {
    live.textContent = '';
    window.setTimeout(() => { live.textContent = text; }, 30);
  }

  function pinItem(item) {
    const url = webUrl(item.url);
    if (!url) return null;
    const name = titleOf(item);
    const cat = CATEGORIES.includes(item.category) ? item.category : 'web';
    const li = element('li', 'pin');
    li.dataset.url = url;
    const link = element('a', 'pin-link');
    link.href = url;
    link.title = name;
    const tile = element('span', 'tile cat-' + cat);
    tile.appendChild(svgUse('g-' + cat, 'glyph'));
    link.appendChild(tile);
    link.appendChild(element('span', 'pin-label', name));
    openLike(link, url);
    // Delete takes a page off the row from the keyboard; the button does the same for the mouse.
    link.addEventListener('keydown', (e) => {
      if (e.key === 'Delete' || e.key === 'Backspace') {
        e.preventDefault();
        unpin(li, name);
      }
    });
    li.appendChild(link);
    const remove = element('button', 'pin-remove');
    remove.type = 'button';
    remove.tabIndex = -1;
    remove.setAttribute('aria-label', fmt(strings.removeFromPinned, name));
    remove.title = fmt(strings.removeFromPinned, name);
    remove.appendChild(svgUse('g-remove', 'glyph'));
    remove.addEventListener('click', () => unpin(li, name));
    li.appendChild(remove);
    return li;
  }

  function unpin(li, name) {
    const next = li.nextElementSibling || li.previousElementSibling;
    Promise.resolve(host.unpin(li.dataset.url)).then(() => {
      li.remove();
      syncPinned();
      announce(fmt(strings.removedFromPinned, name));
      const target = next && next.querySelector('.pin-link');
      (target || query).focus();
    }).catch(() => {});
  }

  function syncPinned() {
    pinsEmpty.hidden = pins.children.length !== 0;
  }

  function recentItem(item) {
    const url = webUrl(item.url);
    if (!url) return null;
    const li = element('li');
    const link = element('a', 'recent');
    link.href = url;
    link.appendChild(svgUse('g-doc', 'glyph'));
    const text = element('span', 'recent-text');
    text.appendChild(element('span', 'recent-title', titleOf(item)));
    text.appendChild(element('span', 'recent-when', openedText(item.when)));
    link.appendChild(text);
    openLike(link, url);
    li.appendChild(link);
    return li;
  }

  function render(state) {
    pins.replaceChildren();
    (Array.isArray(state.pinned) ? state.pinned : []).slice(0, MAX_PINNED).forEach((p) => {
      const li = pinItem(p);
      if (li) pins.appendChild(li);
    });
    syncPinned();

    recents.replaceChildren();
    (Array.isArray(state.recent) ? state.recent : []).slice(0, MAX_RECENT).forEach((r) => {
      const li = recentItem(r);
      if (li) recents.appendChild(li);
    });
    const any = recents.children.length > 0;
    recentSection.hidden = !any;
    recentEmpty.hidden = any;

    // One quiet line, or none. A window that shows its own strip says it already.
    status.hidden = !(state.doNotTrack === true) || state.statusStrip === true;
  }

  // The four blocks enter 30 ms apart (motion.md); the spread is capped at 300 ms.
  function number() {
    let i = 0;
    document.querySelectorAll('[data-enter]').forEach((el) => {
      if (el.hidden) return;
      el.style.setProperty('--i', String(Math.min(i, 10)));
      i += 1;
    });
  }

  function enter(animate) {
    number();
    if (animate && !reduceMotion()) page.classList.add('is-entering');
    page.classList.remove('pending');
  }

  // Enter in the search field hands the text to the host: an address opens, anything else is a
  // search with the default search provider. No suggestions are asked for.
  form.addEventListener('submit', (e) => {
    e.preventDefault();
    const text = query.value.trim();
    if (text) host.submit(text);
  });
  query.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && query.value) {
      e.preventDefault();
      query.value = '';
    }
  });
  emptyAction.addEventListener('click', () => query.focus());

  // "/" jumps to the search field from anywhere on the page, as in a list.
  document.addEventListener('keydown', (e) => {
    if (e.key !== '/' || e.ctrlKey || e.metaKey || e.altKey) return;
    const t = e.target;
    if (t instanceof HTMLInputElement || t instanceof HTMLTextAreaElement || (t instanceof HTMLElement && t.isContentEditable)) return;
    e.preventDefault();
    query.focus();
    query.select();
  });

  applyStrings(null);
  Promise.resolve()
    .then(() => host.getState())
    .then((state) => {
      if (state.reducedMotion === true) document.documentElement.classList.add('reduce-motion');
      applyStrings(state.strings);
      render(state);
      enter(state.firstInWindow !== false);
    })
    .catch(() => {
      // The host did not answer: a calm page with the search field and no pages.
      render({ pinned: [], recent: [], doNotTrack: false });
      enter(false);
    });
})();
