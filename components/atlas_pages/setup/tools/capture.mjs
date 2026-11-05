// SPDX-License-Identifier: BSD-3-Clause
// Headless check of the Atlas first run: walks the four screens with the keyboard only,
// captures each in light and dark, measures text contrast on the rendered page, records
// every request, checks the motion and the reduced-motion path, and saves the
// accessibility tree of each screen.
//
// usage: node tools/capture.mjs <page-url> <out-dir>
//   env PLAYWRIGHT_CORE  path to a playwright-core module (required)
//   env CHROME           browser executable (required)
import { createRequire } from 'node:module';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

const require = createRequire(import.meta.url);
const { chromium } = require(process.env.PLAYWRIGHT_CORE);
const [url, out] = process.argv.slice(2);
if (!url || !out) { console.error('usage: node tools/capture.mjs <page-url> <out-dir>'); process.exit(2); }
mkdirSync(out, { recursive: true });

const NAMES = ['welcome', 'privacy', 'yours', 'start'];
const origin = new URL(url).origin;
const failures = [];
const report = { url, requests: [], console: [], screens: {}, motion: {} };

function fail(msg) { failures.push(msg); }

// Runs in the page: contrast of every visible text against its composited background.
function audit() {
  const cv = document.createElement('canvas'); cv.width = cv.height = 1;
  const cx = cv.getContext('2d', { willReadFrequently: true });
  const rgba = (c) => { cx.clearRect(0, 0, 1, 1); cx.fillStyle = '#000'; cx.fillStyle = c; cx.fillRect(0, 0, 1, 1); const d = cx.getImageData(0, 0, 1, 1).data; return [d[0], d[1], d[2], d[3] / 255]; };
  // canvas premultiplies: read the colour itself from a 1x1 fill over nothing
  const parse = (c) => { if (c === 'transparent' || /rgba\(0, 0, 0, 0\)/.test(c)) return [0, 0, 0, 0]; cx.clearRect(0, 0, 1, 1); cx.fillStyle = c; const a = cx.fillStyle; return rgba(a); };
  const over = (top, bot) => { const a = top[3] + bot[3] * (1 - top[3]); if (a === 0) return [0, 0, 0, 0]; return [0, 1, 2].map((i) => (top[i] * top[3] + bot[i] * bot[3] * (1 - top[3])) / a).concat([a]); };
  const lum = (c) => { const f = (v) => { v /= 255; return v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4; }; return 0.2126 * f(c[0]) + 0.7152 * f(c[1]) + 0.0722 * f(c[2]); };
  const ratio = (a, b) => { const x = lum(a), y = lum(b); return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05); };
  const bgOf = (el) => { const stack = []; for (let n = el; n; n = n.parentElement) { const c = parse(getComputedStyle(n).backgroundColor); if (c[3] > 0) stack.push(c); if (c[3] >= 1) break; } let acc = [255, 255, 255, 1]; for (let i = stack.length - 1; i >= 0; i--) acc = over(stack[i], acc); return acc; };
  const rows = [];
  const scope = [document.querySelector('.screen:not([hidden])'), document.getElementById('step-label')];
  for (const root of scope) {
    for (const el of [root, ...root.querySelectorAll('*')]) {
      if (!el.offsetParent && el !== root) continue;
      const own = Array.from(el.childNodes).filter((n) => n.nodeType === 3).map((n) => n.textContent.trim()).join(' ').trim();
      if (!own) continue;
      const cs = getComputedStyle(el);
      const bg = bgOf(el);
      const fg = over(parse(cs.color), bg);
      rows.push({ text: own.slice(0, 48), size: cs.fontSize, weight: cs.fontWeight, ratio: Math.round(ratio(fg, bg) * 100) / 100, fg: fg.slice(0, 3).map(Math.round), bg: bg.slice(0, 3).map(Math.round) });
    }
  }
  return rows;
}

function describe() {
  const a = document.activeElement;
  if (!a || a === document.body) return 'body';
  const name = a.getAttribute('aria-label') || (a.getAttribute('aria-labelledby') && document.getElementById(a.getAttribute('aria-labelledby').split(' ')[0])?.textContent) || a.textContent.trim() || a.value || '';
  return `${a.tagName.toLowerCase()}${a.getAttribute('role') ? '[role=' + a.getAttribute('role') + ']' : ''}${a.type ? '[' + a.type + ']' : ''} "${name.trim().slice(0, 40)}"`;
}

async function tabTo(page, label, max = 25) {
  const order = [];
  for (let i = 0; i < max; i++) {
    await page.keyboard.press('Tab');
    const d = await page.evaluate(describe);
    order.push(d);
    if (d.includes(`"${label}`)) return order;
  }
  throw new Error(`Tab never reached "${label}"; order: ${order.join(' | ')}`);
}

const browser = await chromium.launch({ executablePath: process.env.CHROME, headless: true, args: ['--no-sandbox', '--disable-gpu', '--font-render-hinting=none'] });

for (const scheme of ['light', 'dark']) {
  const ctx = await browser.newContext({ colorScheme: scheme, viewport: { width: 1100, height: 780 }, deviceScaleFactor: 1 });
  const page = await ctx.newPage();
  page.on('request', (r) => { report.requests.push(r.url()); if (new URL(r.url()).origin !== origin) fail(`request outside the page origin: ${r.url()}`); });
  page.on('console', (m) => { report.console.push(`${scheme} ${m.type()}: ${m.text()}`); if (m.type() === 'error') fail(`console error (${scheme}): ${m.text()}`); });
  page.on('pageerror', (e) => fail(`page error (${scheme}): ${e.message}`));
  await page.goto(url, { waitUntil: 'load' });
  await page.waitForTimeout(800);

  for (let i = 0; i < 4; i++) {
    const name = NAMES[i];
    const visible = await page.evaluate(() => document.querySelector('.screen:not([hidden])').id);
    if (visible !== `screen-${name}`) fail(`${scheme}: expected screen-${name}, saw ${visible}`);
    if (i > 0) {
      const focused = await page.evaluate(() => document.activeElement.tagName);
      if (focused !== 'H1') fail(`${scheme} ${name}: focus after the step is ${focused}, not the heading`);
    }
    await page.waitForTimeout(800); // entrance done (400 ms + 300 ms stagger cap)
    const shot = join(out, `atlas-onboard-${i + 1}-${name}-${scheme}.png`);
    await page.screenshot({ path: shot });
    const rows = await page.evaluate(audit);
    const low = rows.filter((r) => r.ratio < 4.5);
    low.forEach((r) => fail(`${scheme} ${name}: "${r.text}" ${r.ratio}:1 (${r.fg} on ${r.bg})`));
    const entry = { shot, min: Math.min(...rows.map((r) => r.ratio)), texts: rows.length, low };
    if (scheme === 'light') {
      const aria = await page.locator(`#screen-${name}`).ariaSnapshot();
      writeFileSync(join(out, `aria-${i + 1}-${name}.txt`), aria + '\n');
    }
    // keyboard only: Tab to the primary action
    if (i < 3) {
      const primary = i === 0 ? 'Get started' : 'Continue';
      entry.tabOrder = await tabTo(page, primary);
      if (i === 1) {
        // toggle Do Not Track off and on again with Space, from the keyboard
        await page.keyboard.press('Shift+Tab'); // back
        await page.keyboard.press('Shift+Tab'); // skip
        for (let k = 0; k < 3; k++) await page.keyboard.press('Shift+Tab');
        const sw = await page.evaluate(describe);
        await page.keyboard.press('Space');
        const off = await page.evaluate(() => document.querySelector('[data-pref=doNotTrack]').getAttribute('aria-checked'));
        await page.keyboard.press('Space');
        const on = await page.evaluate(() => document.querySelector('[data-pref=doNotTrack]').getAttribute('aria-checked'));
        entry.switchCheck = { focused: sw, afterSpace: off, afterSecondSpace: on };
        if (off !== 'false' || on !== 'true') fail(`${scheme}: Space did not toggle Do Not Track (${sw}: ${off}, ${on})`);
        await tabTo(page, 'Continue');
      }
      await page.keyboard.press('Enter');
    } else {
      // last screen: choose "Bring my things over" with the arrow keys, check the label, finish
      entry.tabOrder = await tabTo(page, 'clean', 10).catch(async () => tabTo(page, 'Start clean', 10));
      await page.keyboard.press('ArrowUp');
      const label = await page.textContent('#finish');
      entry.importLabel = label;
      if (label !== 'Choose a browser…') fail(`${scheme}: primary label with import chosen is "${label}"`);
      if (scheme === 'light') await page.screenshot({ path: join(out, `atlas-onboard-4-start-import-light.png`) });
      await tabTo(page, 'Choose a browser');
      await page.keyboard.press('Enter');
      const status = await page.textContent('#preview-status');
      entry.finish = status;
      if (!status.includes('"start":"import"')) fail(`${scheme}: finish did not hand over the import choice: ${status}`);
    }
    report.screens[`${scheme}-${name}`] = entry;
  }
  await ctx.close();
}

// Narrow window (hig.md section 5: 480 x 360): no screen overflows sideways
{
  const ctx = await browser.newContext({ viewport: { width: 480, height: 360 }, reducedMotion: 'reduce' });
  const page = await ctx.newPage();
  await page.goto(url, { waitUntil: 'load' });
  await page.waitForTimeout(200);
  report.narrow = [];
  for (let i = 0; i < 4; i++) {
    const w = await page.evaluate(() => ({ scroll: document.documentElement.scrollWidth, client: document.documentElement.clientWidth }));
    report.narrow.push(w);
    if (w.scroll > w.client) fail(`narrow ${NAMES[i]}: content is ${w.scroll} px wide in a ${w.client} px window`);
    if (i === 1) await page.screenshot({ path: join(out, 'atlas-onboard-2-privacy-narrow-light.png'), fullPage: true });
    if (i < 3) await page.click('.screen:not([hidden]) [data-action=next]');
  }
  await ctx.close();
}

// Motion: entrance runs on transform/opacity within 400 ms + 300 ms stagger; reduced = none
for (const reducedMotion of ['no-preference', 'reduce']) {
  const ctx = await browser.newContext({ reducedMotion, viewport: { width: 1100, height: 780 } });
  const page = await ctx.newPage();
  await page.goto(url, { waitUntil: 'load' });
  await page.waitForTimeout(100);
  await page.click('[data-action=next]');
  const anims = await page.evaluate(() => document.getAnimations().map((a) => { const t = a.effect.getTiming(); const kf = a.effect.getKeyframes().flatMap((k) => Object.keys(k).filter((p) => !['offset', 'easing', 'composite', 'computedOffset'].includes(p))); return { name: a.animationName, duration: t.duration, delay: t.delay, props: [...new Set(kf)] }; }));
  report.motion[reducedMotion] = { count: anims.length, maxDuration: Math.max(0, ...anims.map((a) => a.duration)), maxDelay: Math.max(0, ...anims.map((a) => a.delay)), props: [...new Set(anims.flatMap((a) => a.props))] };
  const m = report.motion[reducedMotion];
  if (reducedMotion === 'reduce' && m.count !== 0) fail(`reduced motion still runs ${m.count} animations`);
  if (reducedMotion === 'no-preference') {
    if (m.count === 0) fail('no entrance animation ran');
    if (m.maxDuration > 400) fail(`an animation lasts ${m.maxDuration} ms`);
    if (m.maxDelay > 300) fail(`a stagger delay is ${m.maxDelay} ms`);
    const bad = m.props.filter((p) => !['opacity', 'transform'].includes(p));
    if (bad.length) fail(`animated properties beyond transform/opacity: ${bad}`);
  }
  await ctx.close();
}

// Contact sheet for the lead: the four screens, light above, dark below, at half size
{
  const page = await browser.newPage({ viewport: { width: 4 * 550 + 5 * 24, height: 2 * 390 + 3 * 24 + 2 * 28 } });
  const cell = (n, name, scheme) => `<figure><figcaption>${n}. ${name}, ${scheme}</figcaption><img src="data:image/png;base64,${readFileSync(join(out, `atlas-onboard-${n}-${name}-${scheme}.png`)).toString('base64')}"></figure>`;
  const rows = ['light', 'dark'].map((sc) => `<div class="r">${NAMES.map((nm, i) => cell(i + 1, nm, sc)).join('')}</div>`).join('');
  await page.setContent(`<style>body{margin:0;padding:24px;background:#8a8796;font:600 13px Inter,sans-serif;color:#fff}.r{display:flex;gap:24px;margin-bottom:24px}figure{margin:0}figcaption{height:28px}img{width:550px;height:390px;display:block;border-radius:10px}</style>${rows}`);
  await page.screenshot({ path: join(out, 'atlas-onboard-sheet.png') });
  await page.close();
}

await browser.close();
report.failures = failures;
writeFileSync(join(out, 'verify.json'), JSON.stringify(report, null, 2) + '\n');
console.log(`requests: ${report.requests.length} (${[...new Set(report.requests.map((u) => new URL(u).pathname))].join(', ')})`);
for (const [k, v] of Object.entries(report.screens)) console.log(`${k}: ${v.texts} texts, min contrast ${v.min}:1`);
console.log('motion:', JSON.stringify(report.motion));
console.log(failures.length ? `FAIL\n- ${failures.join('\n- ')}` : 'PASS');
process.exit(failures.length ? 1 : 0);
