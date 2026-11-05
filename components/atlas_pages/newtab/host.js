// SPDX-License-Identifier: BSD-3-Clause
// Preview host for the Atlas new tab page. Inside Atlas this file is replaced by the
// engine's adapter of the same name (README.md, "The host contract"), which talks to the
// new tab handler. This one only lets the page run on its own: it keeps nothing and
// reports what the person did on the page.
//
// The preview state is chosen with the query of the page address:
//   ?state=filled (default) | empty | nopins | long
//   &strip=1      a window status strip is showing (the page hides its own line)
//   &dnt=0        Do Not Track is off
//   &enter=0      not the first time this window shows the page (no entrance)
'use strict';

if (!window.atlasNewTab) {
  const q = new URLSearchParams(window.location.search);
  const minute = 60 * 1000;
  const now = Date.now();

  const PINNED = [
    { title: 'Home', url: 'https://home.example/', category: 'web' },
    { title: 'Mail', url: 'https://mail.example/', category: 'talk' },
    { title: 'Files', url: 'https://files.example/', category: 'files' },
    { title: 'Video', url: 'https://video.example/', category: 'media' },
    { title: 'Store', url: 'https://store.example/', category: 'store' },
    { title: 'Settings', url: 'https://settings.example/', category: 'system' },
  ];
  const RECENT = [
    { title: 'Release notes', url: 'https://notes.example/release', when: now - 20 * minute },
    { title: 'Train times', url: 'https://trains.example/', when: now - 26 * 60 * minute },
  ];
  const LONG = [
    { title: 'A page whose title is long enough that it has to be cut off at the end of the card', url: 'https://long.example/a', when: now - 3 * 60 * minute },
    { title: 'Recipes for Sunday', url: 'https://recipes.example/', when: now - 5 * 24 * 60 * minute },
  ];

  const which = q.get('state') || 'filled';
  let pinned = which === 'empty' || which === 'nopins' ? [] : PINNED.map((p) => Object.assign({}, p));
  if (which === 'long') pinned[0] = { title: 'A long name for a short card', url: 'https://long.example/', category: 'web' };
  const recent = which === 'empty' ? [] : which === 'long' ? LONG : RECENT;

  window.atlasNewTab = {
    preview: true,
    // What the handler sends: the pinned shortcuts (title, address, category), the pages to pick
    // up (title, address, when in ms since 1970), and the facts the page shows or hides.
    getState() {
      return Promise.resolve({
        pinned: pinned.map((p) => Object.assign({}, p)),
        recent: recent.map((r) => Object.assign({}, r)),
        doNotTrack: q.get('dnt') !== '0',
        statusStrip: q.get('strip') === '1',
        firstInWindow: q.get('enter') !== '0',
        reducedMotion: null,
      });
    },
    // Opens an address: in this tab, or in a new one for a middle click or Ctrl+click.
    open(url, options) {
      report('open ' + url + (options && options.newTab ? ' in a new tab' : ''));
    },
    // Text typed in the search field: the engine decides between an address and a search
    // with the default search provider.
    submit(text) {
      report('submit ' + text);
    },
    pin(url, title) {
      report('pin ' + url + ' ' + title);
      return Promise.resolve();
    },
    unpin(url) {
      pinned = pinned.filter((p) => p.url !== url);
      report('unpin ' + url);
      return Promise.resolve();
    },
  };

  function report(text) {
    document.documentElement.dataset.lastHostCall = text;
  }
}
