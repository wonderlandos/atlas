// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 The Atlas Authors.
//
// The engine adapter (host_webui.js) against a fake page bridge: what it sends, how it reads
// the answers, what it does when the system cannot say anything about the default browser.
// usage: node --test tests/host_webui.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const SOURCE = readFileSync(new URL('../host_webui.js', import.meta.url), 'utf8');

// A fake page: records chrome.send, answers like the two handlers do, runs timers by hand.
function makePage(answers = {}) {
  const sent = [];
  const timers = [];
  const window = {
    setTimeout(fn, ms) { timers.push({ fn, ms, live: true }); return timers.length - 1; },
    clearTimeout(i) { if (timers[i]) timers[i].live = false; },
    chrome: {
      send(message, args) {
        sent.push({ message, args });
        const answer = answers[message];
        if (answer && typeof args[0] === 'string' && args[0].startsWith('atlas-')) {
          // the engine answers by calling cr.webUIResponse
          Promise.resolve().then(() => answer(window, args));
        }
      },
    },
  };
  vm.runInNewContext(SOURCE, { window, Promise, Map, Array, Object });
  return { window, sent, timers };
}

// Objects made inside the vm context have another prototype: compare plain copies.
const plain = (x) => JSON.parse(JSON.stringify(x));

const ok = (value) => (w, args) => w.cr.webUIResponse(args[0], true, value);
const fail = (value) => (w, args) => w.cr.webUIResponse(args[0], false, value);

test('getState merges the privacy state and the default-browser state', async () => {
  const { window } = makePage({
    'atlasFirstRun.getState': ok({ reducedMotion: null, privacy: { doNotTrack: true, globalPrivacyControl: false, trackerBlocking: true } }),
    requestDefaultBrowserState: ok({ isDefault: false, canBeDefault: true, isDisabledByPolicy: false, isUnknownError: false }),
  });
  const s = await window.atlasFirstRun.getState();
  assert.deepEqual(plain(s), {
    isDefault: false, canBeDefault: true, reducedMotion: null,
    privacy: { doNotTrack: true, globalPrivacyControl: false, trackerBlocking: true },
  });
  assert.equal(window.atlasFirstRun.preview, false);
});

test('the default-browser row stays hidden when the system cannot say or policy forbids', async () => {
  for (const def of [null, { isDefault: false, canBeDefault: true, isDisabledByPolicy: true }, { isDefault: false, canBeDefault: true, isUnknownError: true }]) {
    const { window } = makePage({
      'atlasFirstRun.getState': ok({ privacy: {} }),
      requestDefaultBrowserState: def ? ok(def) : fail('nope'),
    });
    const s = await window.atlasFirstRun.getState();
    assert.equal(s.canBeDefault, false);
  }
});

test('reduced motion is passed through only when it is a boolean', async () => {
  for (const [given, want] of [[true, true], [false, false], ['yes', null], [undefined, null]]) {
    const { window } = makePage({
      'atlasFirstRun.getState': ok({ reducedMotion: given, privacy: {} }),
      requestDefaultBrowserState: ok({ isDefault: true, canBeDefault: false }),
    });
    assert.equal((await window.atlasFirstRun.getState()).reducedMotion, want);
  }
});

test('missing privacy values read as the defaults (on)', async () => {
  const { window } = makePage({
    'atlasFirstRun.getState': ok({}),
    requestDefaultBrowserState: ok({ isDefault: true, canBeDefault: false }),
  });
  const s = await window.atlasFirstRun.getState();
  assert.deepEqual(plain(s.privacy), { doNotTrack: true, globalPrivacyControl: true, trackerBlocking: true });
});

test('finish without the default browser hands the choices over once, at once', async () => {
  const { window, sent } = makePage();
  const choices = { skipped: false, privacy: { doNotTrack: false, globalPrivacyControl: true, trackerBlocking: true }, start: 'import', makeDefault: false };
  await window.atlasFirstRun.finish(choices);
  assert.deepEqual(plain(sent), [{ message: 'atlasFirstRun.finish', args: [choices] }]);
});

test('finish with make default asks the system first and hands over after its answer', async () => {
  const { window, sent } = makePage();
  const choices = { skipped: false, privacy: {}, start: 'clean', makeDefault: true };
  const done = window.atlasFirstRun.finish(choices);
  assert.deepEqual(sent.map((m) => m.message), ['setAsDefaultBrowser']);
  window.cr.webUIListenerCallback('browser-default-state-changed', { isDefault: true });
  await done;
  assert.deepEqual(sent.map((m) => m.message), ['setAsDefaultBrowser', 'atlasFirstRun.finish']);
});

test('finish with make default does not wait forever', async () => {
  const { window, sent, timers } = makePage();
  const done = window.atlasFirstRun.finish({ skipped: false, privacy: {}, start: 'clean', makeDefault: true });
  assert.equal(timers.length, 1);
  assert.equal(timers[0].ms, 2500);
  timers[0].fn();
  await done;
  assert.deepEqual(sent.map((m) => m.message), ['setAsDefaultBrowser', 'atlasFirstRun.finish']);
});

test('a skip never touches the default browser, even if the switch was on', async () => {
  const { window, sent } = makePage();
  await window.atlasFirstRun.finish({ skipped: true, privacy: {}, start: 'clean', makeDefault: true });
  assert.deepEqual(sent.map((m) => m.message), ['atlasFirstRun.finish']);
});

test('answers for unknown ids and listeners without subscribers are ignored', () => {
  const { window } = makePage();
  window.cr.webUIResponse('atlas-999', true, {});
  window.cr.webUIListenerCallback('nobody-listens', 1);
});

test('the file does not replace a host that is already there', () => {
  const window = { atlasFirstRun: { mine: true } };
  vm.runInNewContext(SOURCE, { window, Promise, Map, Array, Object });
  assert.equal(window.atlasFirstRun.mine, true);
});
