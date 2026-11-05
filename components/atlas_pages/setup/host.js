// SPDX-License-Identifier: BSD-3-Clause
// Preview host for the Atlas first run. Inside Atlas this file is replaced by the
// engine's adapter of the same name (README.md, "The host contract"), which talks to
// the first-run handler. This one only lets the page run on its own: it changes
// nothing, keeps nothing, and reports the choices on the page.
'use strict';

if (!window.atlasFirstRun) {
  window.atlasFirstRun = {
    preview: true,
    // The state a fresh profile has once the privacy defaults are applied.
    getState() {
      return Promise.resolve({
        isDefault: false,
        canBeDefault: true,
        reducedMotion: null,
        privacy: { doNotTrack: true, globalPrivacyControl: true, trackerBlocking: true },
      });
    },
    finish(choices) {
      const out = document.getElementById('preview-status');
      if (out) {
        out.textContent = 'Preview, nothing was changed: ' + JSON.stringify(choices);
      }
    },
  };
}
