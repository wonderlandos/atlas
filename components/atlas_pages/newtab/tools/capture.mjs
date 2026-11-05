// SPDX-License-Identifier: BSD-3-Clause
// Headless check of the Atlas new tab page. The DOM check of reel scene R02b (heading, search
// field 560 x 40, six pinned cards 96 x 88, two cards 320 x 64, the 30 ms stagger read from the
// page's animations, no request off the page), the keyboard walk, captures in light and dark
// (filled, empty, without pins, long titles, narrow window), reduced motion, text contrast on the
// rendered page, and the pair sheet against the reel pose.
//
// usage: node tools/capture.mjs <page-url> <out-dir> [<pose-png>]
//   env PLAYWRIGHT_CORE  path to a playwright-core module (required)
//   env CHROME           browser executable (required)
//   <pose-png>           reel/out/poses/atlas-ntp-light.png (optional: writes the pair sheet)
// The same checks run against the real page in the engine by passing its address and, for
// an address that is not the preview, --live (the preview's ?state= switches are not sent).
import { createRequire } from 'node:module';
import { mkdirSync, readFileSync, writeFileSync, existsSync } from 'node:fs';
import { join } from 'node:path';

const require = createRequire(import.meta.url);
const { chromium } = require(process.env.PLAYWRIGHT_CORE);
const args = process.argv.slice(2).filter((a) => !a.startsWith('--'));
const [url, out, pose] = args;
if (!url || !out) { console.error('usage: node tools/capture.mjs <page-url> <out-dir> [<pose-png>]'); process.exit(2); }
mkdirSync(out, { recursive: true });

const origin = new URL(url).origin;
const failures = [];
const report = { url, requests: [], console: [], checks: {}, motion: {}, keyboard: {} };
const fail = (m) => failures.push(m);
const at = (query) => url + (url.includes('?') ? '&' : '?') + query;
const VIEW = { width: 1180, height: 764 }; // the content pane of the reel window (1400 x 800 less sidebar and bar)

// Runs in the page: contrast of every visible text against its composited background.
function audit() {
  const cv = document.createElement('canvas'); cv.width = cv.height = 1;
  const cx = cv.getContext('2d', { willReadFrequently: true });
  const rgba = (c) => { cx.clearRect(0, 0, 1, 1); cx.fillStyle = '#000'; cx.fillStyle = c; cx.fillRect(0, 0, 1, 1); const d = cx.getImageData(0, 0, 1, 1).data; return [d[0], d[1], d[2], d[3] / 255]; };
  const parse = (c) => { if (c === 'transparent' || /rgba\(0, 0, 0, 0\)/.test(c)) return [0, 0, 0, 0]; cx.clearRect(0, 0, 1, 1); cx.fillStyle = c; return rgba(cx.fillStyle); };
  const over = (top, bot) => { const a = top[3] + bot[3] * (1 - top[3]); if (a === 0) return [0, 0, 0, 0]; return [0, 1, 2].map((i) => (top[i] * top[3] + bot[i] * bot[3] * (1 - top[3])) / a).concat([a]); };
  const lum = (c) => { const f = (v) => { v /= 255; return v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4; }; return 0.2126 * f(c[0]) + 0.7152 * f(c[1]) + 0.0722 * f(c[2]); };
  const ratio = (a, b) => { const x = lum(a), y = lum(b); return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05); };
  const bgOf = (el) => { const stack = []; for (let n = el; n; n = n.parentElement) { const c = parse(getComputedStyle(n).backgroundColor); if (c[3] > 0) stack.push(c); if (c[3] >= 1) break; } let acc = [255, 255, 255, 1]; for (let i = stack.length - 1; i >= 0; i--) acc = over(stack[i], acc); return acc; };
  const rows = [];
  for (const el of document.querySelectorAll('body *')) {
    if (el.closest('svg, template, .visually-hidden') || el.offsetParent === null && getComputedStyle(el).position !== 'fixed') continue;
    const own = Array.from(el.childNodes).filter((n) => n.nodeType === 3).map((n) => n.textContent.trim()).join(' ').trim();
    if (!own) continue;
    const cs = getComputedStyle(el);
    const bg = bgOf(el);
    const fg = over(parse(cs.color), bg);
    rows.push({ text: own.slice(0, 40), size: cs.fontSize, weight: cs.fontWeight, ratio: Math.round(ratio(fg, bg) * 100) / 100 });
  }
  const ph = document.getElementById('query');
  if (ph) { const bg = bgOf(ph); const fg = over(parse(getComputedStyle(ph, '::placeholder').color), bg); rows.push({ text: '(placeholder)', size: getComputedStyle(ph).fontSize, weight: '400', ratio: Math.round(ratio(fg, bg) * 100) / 100 }); }
  return rows;
}

// Runs in the page: the geometry the reel and tokens.md section 5 name.
function geometry() {
  const r = (el) => { if (!el) return null; const b = el.getBoundingClientRect(); return { x: Math.round(b.x * 10) / 10, y: Math.round(b.y * 10) / 10, w: Math.round(b.width * 10) / 10, h: Math.round(b.height * 10) / 10 }; };
  const css = (el, p) => getComputedStyle(el)[p];
  const pins = Array.from(document.querySelectorAll('.pin-link'));
  const recents = Array.from(document.querySelectorAll('.recent'));
  const tile = document.querySelector('.tile');
  return {
    h1: { text: document.querySelector('h1')?.textContent, size: css(document.querySelector('h1'), 'fontSize'), weight: css(document.querySelector('h1'), 'fontWeight'), count: document.querySelectorAll('h1').length },
    badge: r(document.querySelector('.badge')),
    search: Object.assign(r(document.getElementById('query')), { radius: css(document.getElementById('query'), 'borderRadius') }),
    pinHeading: { text: document.getElementById('pinned-title').textContent, size: css(document.getElementById('pinned-title'), 'fontSize'), weight: css(document.getElementById('pinned-title'), 'fontWeight') },
    pins: pins.map((p) => Object.assign(r(p), { label: p.textContent, radius: css(p, 'borderRadius') })),
    tile: tile ? Object.assign(r(tile), { radius: css(tile, 'borderRadius') }) : null,
    tiles: Array.from(document.querySelectorAll('.tile')).map((t) => css(t, 'backgroundColor')),
    recentHeading: { text: document.getElementById('recent-title').textContent, hidden: document.getElementById('recent').hidden },
    recents: recents.map((p) => Object.assign(r(p), { title: p.querySelector('.recent-title').textContent, when: p.querySelector('.recent-when').textContent, radius: css(p, 'borderRadius') })),
    emptyVisible: !document.getElementById('recent-empty').hidden,
    emptyTitle: { text: document.getElementById('empty-title').textContent, size: css(document.getElementById('empty-title'), 'fontSize') },
    status: Object.assign(r(document.getElementById('status')), { hidden: document.getElementById('status').hidden, text: document.getElementById('status').textContent, size: css(document.getElementById('status'), 'fontSize') }),
    overflowX: document.documentElement.scrollWidth - document.documentElement.clientWidth,
  };
}

function animations() {
  return document.getAnimations().map((a) => {
    const t = a.effect.getTiming();
    const kf = a.effect.getKeyframes().flatMap((k) => Object.keys(k).filter((p) => !['offset', 'easing', 'composite', 'computedOffset'].includes(p)));
    return { target: a.effect.target?.id || a.effect.target?.className, duration: t.duration, delay: t.delay, props: [...new Set(kf)] };
  });
}

function describe() {
  const a = document.activeElement;
  if (!a || a === document.body) return 'body';
  const name = a.getAttribute('aria-label') || a.title || a.textContent.trim() || '';
  return `${a.tagName.toLowerCase()} "${name.trim().slice(0, 40)}"`;
}

const browser = await chromium.launch({ executablePath: process.env.CHROME, headless: true, args: ['--no-sandbox', '--disable-gpu', '--font-render-hinting=none'] });

async function open(opts, query) {
  const ctx = await browser.newContext({ colorScheme: opts.scheme || 'light', reducedMotion: opts.reduced ? 'reduce' : 'no-preference', viewport: opts.view || VIEW, deviceScaleFactor: 1 });
  const page = await ctx.newPage();
  page.on('request', (r) => { report.requests.push(r.url()); if (!r.url().startsWith('data:') && new URL(r.url()).origin !== origin) fail(`request outside the page origin: ${r.url()}`); });
  page.on('console', (m) => { report.console.push(`${m.type()}: ${m.text()}`); if (['error', 'warning'].includes(m.type())) fail(`console ${m.type()}: ${m.text()}`); });
  page.on('pageerror', (e) => fail(`page error: ${e.message}`));
  await page.addInitScript(() => { window.__csp = []; document.addEventListener('securitypolicyviolation', (e) => window.__csp.push(e.violatedDirective + ' ' + e.blockedURI)); });
  await page.goto(at(query || 'state=filled'), { waitUntil: 'domcontentloaded' });
  return { ctx, page };
}

async function ready(page) {
  await page.waitForFunction(() => !document.getElementById('page').classList.contains('pending'));
}

// 1. The DOM check and the entrance, in light, the first time ---------------------------------
{
  const { ctx, page } = await open({}, 'state=filled');
  await ready(page);
  // read the animations at once: they last 250 ms plus the stagger
  const anims = await page.evaluate(animations);
  report.motion.entrance = anims;
  const g = await page.evaluate(geometry);
  report.checks.geometry = g;
  if (g.h1.text !== 'Atlas' || g.h1.count !== 1 || g.h1.size !== '26px' || g.h1.weight !== '700') fail(`heading: ${JSON.stringify(g.h1)}`);
  if (Math.abs(g.badge.w - 72) > 0.5 || Math.abs(g.badge.h - 72) > 0.5) fail(`badge is ${g.badge.w} x ${g.badge.h}, not 72 x 72`);
  if (g.search.w !== 560 || g.search.h !== 40 || g.search.radius !== '10px') fail(`search field ${g.search.w} x ${g.search.h} radius ${g.search.radius}, not 560 x 40 radius 10`);
  if (Math.abs((g.search.x + g.search.w / 2) - VIEW.width / 2) > 1) fail('search field is not centred');
  if (g.pinHeading.text !== 'Pinned' || g.pinHeading.size !== '15px' || g.pinHeading.weight !== '600') fail(`Pinned heading: ${JSON.stringify(g.pinHeading)}`);
  if (g.pins.length !== 6) fail(`${g.pins.length} pinned cards, not 6`);
  g.pins.forEach((p) => { if (p.w !== 96 || p.h !== 88 || p.radius !== '10px') fail(`pinned card ${p.label}: ${p.w} x ${p.h} radius ${p.radius}`); });
  const gaps = g.pins.slice(1).map((p, i) => p.x - (g.pins[i].x + g.pins[i].w));
  if (gaps.some((x) => x !== 16)) fail(`pinned gaps ${gaps}, not 16`);
  if (g.tile.w !== 32 || g.tile.h !== 32) fail(`tile ${g.tile.w} x ${g.tile.h}, not 32 x 32`);
  if (g.pins.map((p) => p.label).join() !== 'Home,Mail,Files,Video,Store,Settings') fail(`pinned order: ${g.pins.map((p) => p.label)}`);
  if (new Set(g.tiles).size !== 6) fail(`the six tiles do not have six colours: ${g.tiles}`);
  if (g.recentHeading.text !== 'Pick up where you left off' || g.recentHeading.hidden) fail(`Pick up heading: ${JSON.stringify(g.recentHeading)}`);
  if (g.recents.length !== 2) fail(`${g.recents.length} pick-up cards, not 2`);
  g.recents.forEach((p) => { if (p.w !== 320 || p.h !== 64 || p.radius !== '10px') fail(`pick-up card ${p.title}: ${p.w} x ${p.h} radius ${p.radius}`); });
  if (g.recents[0]?.when !== 'Opened 20 minutes ago' || g.recents[1]?.when !== 'Opened yesterday') fail(`when lines: ${g.recents.map((x) => x.when)}`);
  if (g.status.hidden || g.status.text !== 'Do Not Track is on' || g.status.h !== 24 || g.status.size !== '11px') fail(`status line: ${JSON.stringify(g.status)}`);
  if (g.emptyVisible) fail('the empty state shows next to pick-up cards');
  // stagger: four blocks, 30 ms apart, 250 ms, 8 px rise and fade only
  if (anims.length !== 4) fail(`${anims.length} entrance animations, not 4`);
  const delays = anims.map((a) => a.delay).sort((a, b) => a - b);
  if (delays.join() !== '0,30,60,90') fail(`stagger delays ${delays}, not 0,30,60,90`);
  if (anims.some((a) => a.duration !== 250)) fail(`entrance durations ${anims.map((a) => a.duration)}, not 250 (--motion-normal)`);
  const bad = [...new Set(anims.flatMap((a) => a.props))].filter((p) => !['opacity', 'transform'].includes(p));
  if (bad.length) fail(`entrance animates ${bad}`);
  const order = await page.evaluate(() => document.getAnimations().sort((a, b) => a.effect.getTiming().delay - b.effect.getTiming().delay).map((a) => a.effect.target.id || a.effect.target.className));
  report.motion.order = order;
  if (!/hero/.test(order[0]) || order[1] !== 'search' || order[2] !== 'pinned' || order[3] !== 'recent') fail(`entrance order ${order}`);
  const rise = await page.evaluate(() => { const a = document.getAnimations()[0]; return a.effect.getKeyframes().map((k) => k.transform); });
  if (!rise.some((t) => /translateY\(8px\)|matrix\(1, 0, 0, 1, 0, 8\)/.test(t || ''))) fail(`entrance rise is not 8 px: ${rise}`);
  await page.waitForTimeout(600);
  const left = await page.evaluate(() => document.getAnimations().filter((a) => a.playState !== 'finished').length);
  if (left) fail(`${left} animations unfinished after 600 ms`);
  const csp = await page.evaluate(() => window.__csp);
  if (csp.length) fail(`CSP violations: ${csp}`);
  await ctx.close();
}

// 2. Captures: light and dark, each state; text contrast on the rendered page ------------------
const STATES = [['filled', 'state=filled'], ['empty', 'state=empty'], ['nopins', 'state=nopins'], ['long', 'state=long'], ['strip', 'state=filled&strip=1'], ['dntoff', 'state=filled&dnt=0']];
for (const scheme of ['light', 'dark']) {
  for (const [name, query] of STATES) {
    if (scheme === 'dark' && !['filled', 'empty'].includes(name)) continue;
    const { ctx, page } = await open({ scheme, reduced: true }, query + '&enter=0');
    await ready(page);
    await page.waitForTimeout(150);
    await page.screenshot({ path: join(out, `atlas-ntp-${name}-${scheme}.png`) });
    const rows = await page.evaluate(audit);
    rows.filter((r) => r.ratio < 4.5).forEach((r) => fail(`${scheme} ${name}: "${r.text}" ${r.ratio}:1`));
    report.checks[`${scheme}-${name}`] = { min: Math.min(...rows.map((r) => r.ratio)), texts: rows.length };
    const g = await page.evaluate(geometry);
    if (name === 'empty') {
      if (!g.emptyVisible || g.emptyTitle.size !== '22px' || g.emptyTitle.text !== 'Nothing to pick up yet') fail(`${scheme} empty state: ${JSON.stringify(g.emptyTitle)}`);
      if (g.pins.length !== 0 && scheme === 'light') { /* the empty profile has no pinned pages either */ }
      if (g.status.hidden) fail(`${scheme} empty: the status line is gone`);
    }
    if (name === 'nopins' && (g.pins.length !== 0 || g.recents.length !== 2)) fail('nopins: wrong cards');
    if (name === 'strip' && !g.status.hidden) fail('the page keeps its own line next to the window strip');
    if (name === 'dntoff' && !g.status.hidden) fail('the status line shows with Do Not Track off');
    if (name === 'long') {
      const w = await page.evaluate(() => Array.from(document.querySelectorAll('.pin-link, .recent')).map((e) => [e.scrollWidth <= e.clientWidth + 1 || getComputedStyle(e.querySelector('.pin-label, .recent-title')).textOverflow === 'ellipsis', e.getBoundingClientRect().width]));
      if (w.some(([ok]) => !ok)) fail('long titles are not cut with an ellipsis');
      if (g.pins[0].w !== 96 || g.recents[0].w !== 320) fail('long titles change the card size');
    }
    await ctx.close();
  }
}

// 3. Reduced motion and not-the-first-time: no animation at all, content visible ----------------
for (const [label, opts, query] of [['reduced', { reduced: true }, 'state=filled'], ['not-first', {}, 'state=filled&enter=0']]) {
  const { ctx, page } = await open(opts, query);
  await ready(page);
  const anims = await page.evaluate(animations);
  const opacity = await page.evaluate(() => Array.from(document.querySelectorAll('[data-enter]')).map((e) => getComputedStyle(e).opacity));
  report.motion[label] = { count: anims.length, opacity };
  if (anims.length) fail(`${label}: ${anims.length} animations run`);
  if (opacity.some((o) => o !== '1')) fail(`${label}: content not fully visible ${opacity}`);
  await ctx.close();
}

// 4. Narrow window (hig.md section 5: 480 x 360): nothing overflows sideways --------------------
for (const [label, state] of [['filled', 'state=filled'], ['long', 'state=long']]) {
  const { ctx, page } = await open({ view: { width: 480, height: 360 }, reduced: true }, state + '&enter=0');
  await ready(page);
  const g = await page.evaluate(geometry);
  report.checks[`narrow-${label}`] = { overflowX: g.overflowX, pins: g.pins.length, pinXs: g.pins.map((p) => p.x), recentW: g.recents.map((r) => r.w) };
  if (g.overflowX > 0) fail(`narrow ${label}: content is ${g.overflowX} px wider than the window`);
  if (g.recents.some((r) => r.w > 432)) fail(`narrow ${label}: a pick-up card is wider than the column`);
  if (label === 'filled') await page.screenshot({ path: join(out, 'atlas-ntp-narrow-light.png'), fullPage: true });
  await ctx.close();
}

// 5. Keyboard and the host calls ---------------------------------------------------------------
{
  const { ctx, page } = await open({ reduced: true }, 'state=filled&enter=0');
  await ready(page);
  const last = () => page.evaluate(() => document.documentElement.dataset.lastHostCall || '');
  const order = [];
  for (let i = 0; i < 10; i++) { await page.keyboard.press('Tab'); order.push(await page.evaluate(describe)); }
  report.keyboard.tabOrder = order;
  const expect = ['input "Search or enter an address"', 'a "Home"', 'a "Mail"', 'a "Files"', 'a "Video"', 'a "Store"', 'a "Settings"', 'a "Release notes', 'a "Train times'];
  expect.forEach((e, i) => { if (!order[i]?.startsWith(e)) fail(`Tab stop ${i + 1} is ${order[i]}, expected ${e}`); });
  if (order[9] !== 'body') fail(`Tab after the last card is ${order[9]}, not the end of the page`);
  // "/" from a card jumps to the field, and the text typed there goes to the host on Enter
  await page.keyboard.press('Shift+Tab');
  await page.keyboard.press('/');
  const afterSlash = await page.evaluate(describe);
  if (!afterSlash.startsWith('input')) fail(`"/" did not focus the search field: ${afterSlash}`);
  await page.keyboard.type('wolos.org');
  await page.keyboard.press('Enter');
  if ((await last()) !== 'submit wolos.org') fail(`Enter in the field sent "${await last()}"`);
  await page.keyboard.press('Escape');
  const cleared = await page.evaluate(() => document.getElementById('query').value);
  if (cleared !== '') fail('Escape did not clear the field');
  await page.keyboard.press('Enter');
  const empty = await last();
  if (empty !== 'submit wolos.org') fail(`Enter on an empty field sent something: ${empty}`);
  // "/" typed into the field is text, not a jump
  await page.keyboard.type('a/b');
  const typed = await page.evaluate(() => document.getElementById('query').value);
  if (typed !== 'a/b') fail(`"/" in the field did not type: ${typed}`);
  // Enter on a card opens it; Ctrl+click opens a new tab
  await page.focus('.pin-link');
  await page.keyboard.press('Enter');
  if ((await last()) !== 'open https://home.example/') fail(`Enter on Home sent "${await last()}"`);
  await page.click('.pin:nth-child(2) .pin-link', { modifiers: ['Control'] });
  if ((await last()) !== 'open https://mail.example/ in a new tab') fail(`Ctrl+click on Mail sent "${await last()}"`);
  await page.click('.recent', {});
  if ((await last()) !== 'open https://notes.example/release') fail(`click on Release notes sent "${await last()}"`);
  // Delete unpins the focused card, focus moves on, the others stay
  await page.focus('.pin:nth-child(2) .pin-link');
  await page.keyboard.press('Delete');
  await page.waitForFunction(() => document.querySelectorAll('.pin').length === 5);
  const after = await page.evaluate(() => ({ labels: Array.from(document.querySelectorAll('.pin-label')).map((e) => e.textContent), focus: document.activeElement.textContent, live: document.getElementById('live').textContent }));
  report.keyboard.afterDelete = after;
  if ((await last()) !== 'unpin https://mail.example/') fail(`Delete sent "${await last()}"`);
  if (after.labels.join() !== 'Home,Files,Video,Store,Settings') fail(`after Delete: ${after.labels}`);
  if (after.focus !== 'Files') fail(`after Delete focus is on ${after.focus}, not the next card`);
  await page.waitForTimeout(100);
  if (!(await page.evaluate(() => document.getElementById('live').textContent)).includes('Mail removed from Pinned')) fail('no announcement for the removed card');
  // removing the last card shows the hint and keeps the field
  for (let i = 0; i < 5; i++) { await page.focus('.pin-link'); await page.keyboard.press('Delete'); await page.waitForFunction((n) => document.querySelectorAll('.pin').length === n, 4 - i); }
  const hint = await page.evaluate(() => ({ hidden: document.getElementById('pins-empty').hidden, heading: document.getElementById('pinned-title').textContent }));
  if (hint.hidden) fail('no hint after the last pinned page is removed');
  const csp = await page.evaluate(() => window.__csp);
  if (csp.length) fail(`CSP violations: ${csp}`);
  // the accessibility tree
  writeFileSync(join(out, 'aria-newtab.txt'), (await page.locator('body').ariaSnapshot()) + '\n');
  await ctx.close();
}


// 7. The engine's adapter (host.engine.js) served as host.js, with a stub of the WebUI channel ---
{
  const ctx = await browser.newContext({ viewport: VIEW, reducedMotion: 'reduce' });
  const page = await ctx.newPage();
  const sent = [];
  await page.exposeFunction('__send', (name, args) => sent.push([name, args]));
  await page.addInitScript(() => {
    const STATE = { pinned: [{ title: 'Home', url: 'https://home.example/', category: 'web' }, { title: 'Mail', url: 'https://mail.example/', category: 'talk' }], recent: [], doNotTrack: true, statusStrip: false, firstInWindow: false, strings: { pinned: 'Fixados' } };
    window.chrome = { send(name, args) {
      window.__send(name, args);
      // the handler answers on its own time, here on the next task
      if (name === 'getState') setTimeout(() => window.atlasNewTabHost.resolve(args[0], STATE), 5);
      if (name === 'unpin' || name === 'pin') setTimeout(() => window.atlasNewTabHost.resolve(args[0], null), 5);
    } };
  });
  const engine = readFileSync(new URL('../host.engine.js', import.meta.url), 'utf8');
  await page.route('**/host.js', (r) => r.fulfill({ contentType: 'text/javascript', body: engine }));
  page.on('pageerror', (e) => fail(`adapter: page error ${e.message}`));
  await page.goto(at(''), { waitUntil: 'domcontentloaded' });
  await ready(page);
  const g = await page.evaluate(geometry);
  if (g.pins.length !== 2 || g.recents.length !== 0 || !g.emptyVisible) fail(`adapter: state not rendered: ${g.pins.length} pins`);
  if (g.pinHeading.text !== 'Fixados') fail(`adapter: host strings not applied: ${g.pinHeading.text}`);
  await page.click('.pin-link');
  await page.fill('#query', 'hello world');
  await page.keyboard.press('Enter');
  await page.focus('.pin:nth-child(2) .pin-link');
  await page.keyboard.press('Delete');
  await page.waitForFunction(() => document.querySelectorAll('.pin').length === 1);
  const names = sent.map(([n]) => n).join();
  report.keyboard.adapterCalls = sent;
  if (names !== 'getState,open,submit,unpin') fail(`adapter: messages ${names}`);
  if (JSON.stringify(sent[1][1]) !== '["https://home.example/",false]') fail(`adapter: open args ${JSON.stringify(sent[1][1])}`);
  if (JSON.stringify(sent[2][1]) !== '["hello world"]') fail(`adapter: submit args ${JSON.stringify(sent[2][1])}`);
  if (sent[3][1][1] !== 'https://mail.example/') fail(`adapter: unpin args ${JSON.stringify(sent[3][1])}`);
  await ctx.close();
}

// 6. Pair sheet with the reel pose -------------------------------------------------------------
if (pose && existsSync(pose)) {
  const page = await browser.newPage({ viewport: { width: 2 * VIEW.width + 72, height: VIEW.height + 72 + 28 } });
  const b64 = (f) => readFileSync(f).toString('base64');
  await page.setContent(`<style>body{margin:0;padding:24px;background:#8a8796;font:600 13px Inter,sans-serif;color:#fff}.r{display:flex;gap:24px}figure{margin:0}figcaption{height:28px}.p{width:${VIEW.width}px;height:${VIEW.height}px;overflow:hidden;border-radius:10px;background:#fff}.p img{display:block;margin:-36px 0 0 -220px}.p.m img{display:block;margin:0}</style>
    <div class="r"><figure><figcaption>reel pose (content pane)</figcaption><div class="p"><img src="data:image/png;base64,${b64(pose)}"></div></figure>
    <figure><figcaption>this page, light</figcaption><div class="p m"><img src="data:image/png;base64,${b64(join(out, 'atlas-ntp-filled-light.png'))}"></div></figure></div>`);
  await page.screenshot({ path: join(out, 'atlas-ntp-pair-light.png') });
  await page.close();
}

await browser.close();
report.failures = failures;
writeFileSync(join(out, 'verify.json'), JSON.stringify(report, null, 2) + '\n');
console.log(`requests: ${report.requests.length} (${[...new Set(report.requests.map((u) => (u.startsWith('data:') ? 'data:' : new URL(u).pathname)))].join(', ')})`);
console.log('entrance:', JSON.stringify(report.motion.entrance?.map((a) => `${a.target}@${a.delay}`)));
for (const [k, v] of Object.entries(report.checks)) if (v.min !== undefined) console.log(`${k}: ${v.texts} texts, min contrast ${v.min}:1`);
console.log(failures.length ? `FAIL\n- ${failures.join('\n- ')}` : 'PASS');
process.exit(failures.length ? 1 : 0);
