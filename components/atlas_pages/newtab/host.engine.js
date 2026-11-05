// SPDX-License-Identifier: BSD-3-Clause
// The engine's adapter of the new tab page: served under the name host.js by the page's data
// source (the preview host of the same name is replaced by this file). It talks to the new tab
// handler over the WebUI message channel. The handler answers a call by running
// atlasNewTabHost.resolve(id, value) in this page.
'use strict';

(function () {
  const pending = new Map();
  let next = 1;

  function call(name, args) {
    return new Promise((resolve) => {
      const id = next++;
      pending.set(id, resolve);
      chrome.send(name, [id].concat(args || []));
    });
  }

  window.atlasNewTabHost = {
    resolve(id, value) {
      const resolve = pending.get(id);
      if (!resolve) return;
      pending.delete(id);
      resolve(value);
    },
  };

  // The contract of the page (README.md, "The host contract").
  window.atlasNewTab = {
    getState() { return call('getState'); },
    open(url, options) { chrome.send('open', [String(url), !!(options && options.newTab)]); },
    submit(text) { chrome.send('submit', [String(text)]); },
    pin(url, title) { return call('pin', [String(url), String(title || '')]); },
    unpin(url) { return call('unpin', [String(url)]); },
  };
})();
