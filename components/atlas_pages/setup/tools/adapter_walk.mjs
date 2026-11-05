// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 The Atlas Authors.
//
// The real page with the real engine adapter (host_webui.js served as host.js) in a headless
// browser, with a fake page bridge standing in for the engine: it answers like the two
// handlers do. Walks the page with the keyboard only and checks what the adapter sends to the
// engine, in which order, and that nothing is requested off the page.
//
// usage: node tools/adapter_walk.mjs
//   env PLAYWRIGHT_CORE  path to a playwright-core module (required)
//   env CHROME           browser executable (required)
import { createRequire } from 'node:module';
import { createServer } from 'node:http';
import { readFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const { chromium } = require(process.env.PLAYWRIGHT_CORE);
const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const failures = [];
const fail = (m) => failures.push(m);
const check = (cond, msg) => { if (!cond) fail(msg); };

// What the engine serves: the same names, host.js is the adapter.
const FILES = {
  '/': ['index.html', 'text/html'], '/index.html': ['index.html', 'text/html'],
  '/theme.css': ['theme.css', 'text/css'], '/onboarding.css': ['onboarding.css', 'text/css'],
  '/onboarding.js': ['onboarding.js', 'text/javascript'], '/host.js': ['host_webui.js', 'text/javascript'],
};
const server = createServer((req, res) => {
  const hit = FILES[new URL(req.url, 'http://x').pathname];
  if (!hit) { res.writeHead(404).end(); return; }
  res.writeHead(200, { 'content-type': hit[1] }).end(readFileSync(join(root, hit[0])));
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const origin = `http://127.0.0.1:${server.address().port}`;

// The fake bridge, installed before the page's scripts run.
function bridge(opts) {
  window.__sent = [];
  const state = { reducedMotion: null, privacy: { doNotTrack: true, globalPrivacyControl: true, trackerBlocking: true } };
  const def = opts.defaultState;
  window.chrome = {
    send(message, args) {
      window.__sent.push({ message, args: JSON.parse(JSON.stringify(args)) });
      const answer = (ok, value) => setTimeout(() => window.cr.webUIResponse(args[0], ok, value), 0);
      if (message === 'atlasFirstRun.getState') answer(!opts.stateFails, opts.stateFails ? 'no' : state);
      if (message === 'requestDefaultBrowserState') answer(true, def);
      if (message === 'setAsDefaultBrowser') {
        setTimeout(() => {
          window.__sent.push({ message: '(event) browser-default-state-changed', args: [] });
          window.cr.webUIListenerCallback('browser-default-state-changed', { ...def, isDefault: true });
        }, 30);
      }
    },
  };
}

const browser = await chromium.launch({ executablePath: process.env.CHROME, headless: true, args: ['--no-sandbox', '--disable-gpu'] });
const NOT_DEFAULT = { isDefault: false, canBeDefault: true, canPin: false, isUnknownError: false, isDisabledByPolicy: false };

async function open(opts = {}) {
  const ctx = await browser.newContext({ viewport: { width: 1100, height: 780 }, reducedMotion: 'reduce' });
  const page = await ctx.newPage();
  const requests = [];
  page.on('request', (r) => requests.push(r.url()));
  page.on('pageerror', (e) => fail(`page error: ${e.message}`));
  page.on('console', (m) => { if (m.type() === 'error') fail(`console error: ${m.text()}`); });
  await page.addInitScript(bridge, { defaultState: NOT_DEFAULT, ...opts });
  await page.goto(origin + '/', { waitUntil: 'load' });
  await page.waitForFunction(() => document.querySelector('.screen:not([hidden])'));
  return { ctx, page, requests };
}
const sent = (page) => page.evaluate(() => window.__sent.map((m) => m.message));
const finishArgs = (page) => page.evaluate(() => (window.__sent.find((m) => m.message === 'atlasFirstRun.finish') || {}).args);
const press = async (page, label, key = 'Enter') => { await page.getByRole(label.role, { name: label.name }).focus(); await page.keyboard.press(key); };
const button = (name) => ({ role: 'button', name });
const sw = (name) => ({ role: 'switch', name });

// 1. Skip from the first screen: one finish message, skipped, nothing about the default browser.
{
  const { ctx, page, requests } = await open();
  await press(page, button('Skip setup'));
  await page.waitForFunction(() => window.__sent.some((m) => m.message === 'atlasFirstRun.finish'));
  const a = (await finishArgs(page))[0];
  check(a.skipped === true && a.makeDefault === false && a.start === 'clean', `skip: ${JSON.stringify(a)}`);
  check(!(await sent(page)).includes('setAsDefaultBrowser'), 'skip asked for the default browser');
  await press(page, button('Skip setup')).catch(() => {});
  check((await sent(page)).filter((m) => m === 'atlasFirstRun.finish').length === 1, 'skip handed over more than once');
  check(requests.every((u) => u.startsWith(origin)), `skip: request off the page: ${requests}`);
  await ctx.close();
}

// 1b. Skip from the second screen after turning a switch off: the switch is handed over as shown.
{
  const { ctx, page } = await open();
  await press(page, button('Get started'));
  await press(page, sw(/Do Not Track/), 'Space');
  await page.locator('.screen:not([hidden]) [data-action=skip]').focus();
  await page.keyboard.press('Enter');
  await page.waitForFunction(() => window.__sent.some((m) => m.message === 'atlasFirstRun.finish'));
  const a = (await finishArgs(page))[0];
  check(a.skipped === true && a.privacy.doNotTrack === false && a.privacy.globalPrivacyControl === true, `skip after a switch: ${JSON.stringify(a)}`);
  await ctx.close();
}

// 2. Full walk by keyboard: GPC off, make default on, start clean.
{
  const { ctx, page, requests } = await open();
  await press(page, button('Get started'));
  await press(page, sw(/Global Privacy Control/), 'Space');
  await press(page, button('Continue'));
  await press(page, button('Continue'));
  check(await page.locator('#default-card').isVisible(), 'the default-browser row is hidden although Atlas can be the default');
  await press(page, sw(/default browser/i), 'Space');
  await press(page, button('Start browsing'));
  await page.waitForFunction(() => window.__sent.some((m) => m.message === 'atlasFirstRun.finish'));
  const messages = await sent(page);
  const asked = messages.indexOf('setAsDefaultBrowser');
  const handed = messages.indexOf('atlasFirstRun.finish');
  const answered = messages.indexOf('(event) browser-default-state-changed');
  check(asked >= 0 && answered > asked && handed > answered, `the hand-over must wait for the system's answer: ${messages}`);
  const a = (await finishArgs(page))[0];
  check(JSON.stringify(a) === JSON.stringify({ skipped: false, privacy: { doNotTrack: true, globalPrivacyControl: false, trackerBlocking: true }, start: 'clean', makeDefault: true }), `full walk: ${JSON.stringify(a)}`);
  check(requests.every((u) => u.startsWith(origin)), `full walk: request off the page: ${requests}`);
  await ctx.close();
}

// 3. Import: the arrow keys choose it, the primary action says so, the choice is handed over.
{
  const { ctx, page } = await open();
  await press(page, button('Get started'));
  await press(page, button('Continue'));
  await press(page, button('Continue'));
  await page.getByRole('radio', { name: /Start clean/ }).focus().catch(() => page.locator('input[value=clean]').focus());
  await page.keyboard.press('ArrowUp');
  check((await page.textContent('#finish')) === 'Choose a browser…', 'the primary action does not say "Choose a browser…"');
  await press(page, button('Choose a browser…'));
  await page.waitForFunction(() => window.__sent.some((m) => m.message === 'atlasFirstRun.finish'));
  const a = (await finishArgs(page))[0];
  check(a.start === 'import' && a.makeDefault === false, `import: ${JSON.stringify(a)}`);
  await ctx.close();
}

// 4. Atlas is already the default (or the system cannot say, or policy forbids): no row.
for (const [why, defaultState, stateFails] of [
  ['already the default', { ...NOT_DEFAULT, isDefault: true }, false],
  ['policy', { ...NOT_DEFAULT, isDisabledByPolicy: true }, false],
  ['unknown error', { ...NOT_DEFAULT, isUnknownError: true }, false],
  ['state fails', NOT_DEFAULT, true],
]) {
  const { ctx, page } = await open({ defaultState, stateFails });
  await press(page, button('Get started'));
  await press(page, button('Continue'));
  await press(page, button('Continue'));
  check(await page.locator('#default-card').isHidden(), `${why}: the default-browser row is shown`);
  await press(page, button('Start browsing'));
  await page.waitForFunction(() => window.__sent.some((m) => m.message === 'atlasFirstRun.finish'));
  check(!(await sent(page)).includes('setAsDefaultBrowser'), `${why}: asked for the default browser`);
  await ctx.close();
}

await browser.close();
server.close();
console.log(failures.length ? `FAIL\n- ${failures.join('\n- ')}` : 'PASS adapter walk: skip, full walk, import, no default-browser row');
process.exit(failures.length ? 1 : 0);
