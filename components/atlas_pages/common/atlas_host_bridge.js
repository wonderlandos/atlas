// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The bridge between an Atlas page and its WebUI message handler. A page's
// adapter, host.js, is built on it and defines the page's host object before
// the page script runs:
//
//   window.atlasFirstRun (setup/host.js), contract in setup/README.md:
//     getState(): Promise<{isDefault, canBeDefault, reducedMotion,
//                          privacy: {doNotTrack, globalPrivacyControl,
//                                    trackerBlocking}}>
//     finish(choices): void
//   window.atlasNewTab (newtab/host.js), contract in newtab/README.md:
//     getState(): Promise<object>   the shape is the new tab page's
//     open(url): void
//
// A preview host (the page opened from disk) defines the same object with
// `preview: true` and never loads this file.
//
// atlasHost.call(name, ...args) sends `name` to the handler with a callback
// id first; the handler answers with ResolveJavascriptCallback or
// RejectJavascriptCallback, which call cr.webUIResponse(id, ok, value).
// atlasHost.send(name, ...args) sends without waiting for an answer.
'use strict';

(() => {
  const pending = new Map();
  let next = 0;

  window.cr = window.cr || {};
  window.cr.webUIResponse = (id, ok, value) => {
    const entry = pending.get(id);
    if (!entry) {
      return;
    }
    pending.delete(id);
    if (ok) {
      entry.resolve(value);
    } else {
      entry.reject(value);
    }
  };
  // Events the browser pushes with FireWebUIListener(name, ...args).
  const listeners = new Map();
  window.cr.webUIListenerCallback = (name, ...args) => {
    for (const fn of listeners.get(name) || []) {
      fn(...args);
    }
  };

  window.atlasHost = Object.freeze({
    call(name, ...args) {
      const id = `atlas-${next++}`;
      return new Promise((resolve, reject) => {
        pending.set(id, {resolve, reject});
        chrome.send(name, [id, ...args]);
      });
    },
    send(name, ...args) {
      chrome.send(name, args);
    },
    on(name, fn) {
      if (!listeners.has(name)) {
        listeners.set(name, []);
      }
      listeners.get(name).push(fn);
    },
  });
})();
