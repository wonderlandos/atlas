// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 The Atlas Authors.
//
// The first-run host inside Atlas. The engine serves this file under the name host.js, in
// place of the preview host of the same name (README.md, "The host contract"). It talks to
// two message handlers on the page's WebUI:
//   - the first-run handler (first_run_handler.cc): the three privacy choices, completion and
//     the hand-over to the next page;
//   - the default-browser handler the settings page already uses: whether Atlas is the default
//     browser, and making it so.
// The page is a document with no helper library, so the two calls the engine makes back into a
// WebUI page (a response, a listener event) are received here.
'use strict';

(function () {
  if (window.atlasFirstRun) return;

  const MSG = {
    state: 'atlasFirstRun.getState',
    finish: 'atlasFirstRun.finish',
    defaultState: 'requestDefaultBrowserState',
    makeDefault: 'setAsDefaultBrowser',
  };
  const DEFAULT_CHANGED = 'browser-default-state-changed';
  // Making Atlas the default browser is one system call; waiting longer than this for its
  // answer does not change the outcome, so the page moves on.
  const DEFAULT_WAIT_MS = 2500;

  const pending = new Map();
  const listeners = new Map();
  let nextId = 1;

  const cr = (window.cr = window.cr || {});
  cr.webUIResponse = function (id, ok, value) {
    const p = pending.get(id);
    if (!p) return;
    pending.delete(id);
    if (ok) p.resolve(value); else p.reject(value);
  };
  cr.webUIListenerCallback = function (name, ...args) {
    (listeners.get(name) || []).forEach((fn) => fn(...args));
  };

  function listen(name, fn) {
    if (!listeners.has(name)) listeners.set(name, []);
    listeners.get(name).push(fn);
    return () => {
      const list = listeners.get(name) || [];
      const i = list.indexOf(fn);
      if (i >= 0) list.splice(i, 1);
    };
  }

  function call(message, ...args) {
    return new Promise((resolve, reject) => {
      const id = 'atlas-' + nextId++;
      pending.set(id, { resolve, reject });
      window.chrome.send(message, [id, ...args]);
    });
  }

  function privacyOf(state) {
    const p = (state && state.privacy) || {};
    return {
      doNotTrack: p.doNotTrack !== false,
      globalPrivacyControl: p.globalPrivacyControl !== false,
      trackerBlocking: p.trackerBlocking !== false,
    };
  }

  // Waits until the system says the default browser changed, or gives up.
  function defaultChanged(wait) {
    return new Promise((resolve) => {
      let stop = () => {};
      const timer = window.setTimeout(() => { stop(); resolve(false); }, wait);
      stop = listen(DEFAULT_CHANGED, () => {
        window.clearTimeout(timer);
        stop();
        resolve(true);
      });
    });
  }

  window.atlasFirstRun = {
    preview: false,

    getState() {
      return Promise.all([
        call(MSG.state),
        // If the system cannot say, the row stays hidden: never offer what cannot be done.
        call(MSG.defaultState).catch(() => null),
      ]).then(([state, def]) => ({
        isDefault: def ? def.isDefault === true : false,
        canBeDefault: !!def && def.canBeDefault === true &&
          def.isDisabledByPolicy !== true && def.isUnknownError !== true,
        reducedMotion: state && typeof state.reducedMotion === 'boolean' ? state.reducedMotion : null,
        privacy: privacyOf(state),
      }));
    },

    finish(choices) {
      const handOver = () => window.chrome.send(MSG.finish, [choices]);
      if (!choices.skipped && choices.makeDefault) {
        const changed = defaultChanged(DEFAULT_WAIT_MS);
        window.chrome.send(MSG.makeDefault, []);
        return changed.then(handOver);
      }
      handOver();
      return Promise.resolve();
    },
  };
})();
