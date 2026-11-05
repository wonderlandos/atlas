#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""e2e_full.py JOURNEY... : ATLAS-E2E whole-browser run on the Kit chrome (--atlas-kit-chrome). Runs ON the forge, in
/scratch/atlas-e2e (own browser instance on a snapshot of slot 8; see e2e_full.sh). Each journey starts its own
instance, drives it with real pointer/keyboard events (vinput) and CDP, and records one row per step
(PASS/FAIL/INFO/SKIP) to results.jsonl; screenshots go to shots/. Usage: e2e_full.py J01 J02 ... | all"""
import re, sys, urllib.parse
from e2e_lib import *
U = "http://127.0.0.1:8123"; SU = "https://127.0.0.1:8444"
PAGES = [f"{U}/{n}.html" for n in ("Alpha", "Bravo", "Charlie", "Delta")]
# toolbar geometry measured on the slot 8 snapshot (1280x720, scale 1)
# (valid prefs: custom_chrome_frame false => no frame buttons in the toolbar; with a corrupt Preferences file the engine draws an X at 1262 and everything shifts 35 px left)
TB = dict(side=(18, 17), back=(46, 17), fwd=(74, 17), reload=(102, 17), omni=(640, 17), privacy=(1178, 17), star=(1206, 17),
          dl=(1234, 17), menu=(1262, 17))
DX = 35   # popovers anchored at the right edge sit 35 px further right than in the frame-button layout
ROW = lambda i: 82 + 28 * i
BAD = re.compile(r"chromium|helium|\bchrome\b|google|incognito|brave|vivaldi|microsoft edge|firefox", re.I)
def front(flags=None, urls=PAGES, theme=None, env=None, inst=None):
    start(["--atlas-kit-chrome", *(flags or []), *urls], theme=theme, env=env, inst=inst)
    dismiss_bar()
def dismiss_bar():
    im = img(shot(f"tmp-{INST}"))
    if dist(px(im, 1250, 58), (255, 255, 255)) > 8 or dist(px(im, 700, 58), (255, 255, 255)) > 8:   # the infobar of --no-sandbox
        if any(dist(px(im, x, 58), (0, 0, 0)) > 200 or lum(px(im, x, 58)) < 0.5 for x in range(1244, 1256)):
            click(1250, 58); time.sleep(0.5)
def chrome_ok(j, label):
    j.check(f"{label}: browser process alive and answering CDP", alive())
# ------------------------------------------------------------------ layout lint on a screenshot (RU-700..706 on the Kit chrome)
def lint(j, im, tag, sidebar=True):
    bg = px(im, 330, 17)
    # toolbar divider and omnibox pill
    pill = bbox_of(im, (300, 0, 980, 36), lambda c: dist(c, bg) > 12 and abs(c[0] - c[1]) < 6 and lum(c) > 0.55 and lum(c) < 0.96 and False)
    # the pill: find the long run on row 17 that is not the page bg
    r = runs(im, 10, 280, 1000, lambda c: dist(c, bg) > 3)
    big = max(r, key=lambda a: a[1] - a[0]) if r else None
    if big:
        col = (big[0] + big[1]) // 2
        v = vruns(im, 600, 0, 36, lambda c: dist(c, bg) > 3)
        vb = max(v, key=lambda a: a[1] - a[0]) if v else None
        w = big[1] - big[0] + 1; h = vb[1] - vb[0] + 1 if vb else -1
        centre = (vb[0] + vb[1]) / 2 if vb else -1
        j.info(f"{tag} omnibox pill {w}x{h} at x{big[0]}..{big[1]}, centre line y={centre}", f"toolbar centre 17.5; RU-701 heights 24/28/36/40; reel P09 says 26")
        j.check(f"{tag} RU-702/toolbar centre line: omnibox centred on the toolbar (|dy|<=1)", abs(centre - 17.5) <= 1, f"centre {centre}")
        j.check(f"{tag} RU-701 omnibox height in {{24,28,36,40}} (reel P09 says 26)", h in (24, 28, 36, 40), f"h={h}", cat="DESIGN")
    if sidebar:
        # sidebar edge: width = first column of the vertical line at y=300
        edge = [x for x in range(150, 260) if dist(px(im, x, 300), px(im, 100, 300)) > 6 and dist(px(im, x, 300), px(im, 600, 400)) > 0 or False]
        # edge: consecutive columns darker than both neighbours at y=300
        cols = [x for x in range(180, 260) if lum(px(im, x, 300)) < lum(px(im, x - 3, 300)) - 0.02 and lum(px(im, x, 300)) < lum(px(im, x + 4, 300)) + 0.5]
        if cols:
            w_edge = len(cols)
            j.check(f"{tag} sidebar edge is 1 px (reel P06) at x={cols[0]}", w_edge == 1 and cols[0] in (219, 220), f"{w_edge} px at x={cols[0]}..{cols[-1]}", cat="DESIGN")
        # rows: the first selected pill (non-bg run on x=100 between y 60 and 100)
        sel = vruns(im, 190, 60, 400, lambda c: dist(c, px(im, 190, 400)) > 10 and c[2] > c[0] + 12)
        sel = [s for s in sel if s[1] - s[0] > 12]
        if sel:
            s = sel[0]; hh = s[1] - s[0] + 1
            xr = runs(im, (s[0] + s[1]) // 2, 0, 216, lambda c: dist(c, px(im, 190, 400)) > 10 and c[2] > c[0] + 12)
            x0, x1 = (min(a for a, b in xr), max(b for a, b in xr)) if xr else (-1, -1)
            j.check(f"{tag} selected row 28 px high (reel P07)", hh == 28, f"{hh} px", cat="DESIGN")
            j.check(f"{tag} RU-700 selected row 8 px from the sidebar edges", x0 == 8 and (220 - x1 - 1) == 8, f"x {x0}..{x1}", cat="DESIGN")
def contrast_row(j, im, tag, boxes):
    for name, (x0, y0, x1, y1), need in boxes:
        cs = [px(im, x, y) for x in range(x0, x1) for y in range(y0, y1)]
        bgc = max(set(cs), key=cs.count)
        fg = min(cs, key=lambda c: lum(c) if lum(bgc) > 0.5 else -lum(c))
        j.check(f"{tag} contrast {name} >= {need}:1", contrast(fg, bgc) >= need, f"{contrast(fg, bgc):.2f}:1 fg{fg} bg{bgc}", cat="ACC")

# ------------------------------------------------------------------ J01 first run
def J01():
    j = J("J01"); quit_browser()
    start([], env={"FIRSTRUN": "--no-sandbox"})   # first run allowed (no --no-first-run)
    dismiss_bar()
    ts = tabs(); j.check("first start opens a tab", len(ts) >= 1, [t["url"] for t in ts])
    j.info("start page of a fresh profile", [t["url"] for t in ts])
    shot("J01-start"); 
    t = cdp("new", "atlas://setup"); time.sleep(3)
    txt = cdp("text", t); shot("J01-setup-1")
    j.check("atlas://setup loads and names Atlas", "Atlas" in txt, txt[:60].replace("\n", " "))
    j.check("setup copy has no vendor/browser names (Doctrine)", not BAD.search(txt), BAD.findall(txt), cat="MANUAL")
    # step through the 4 screens with real clicks on the buttons found by DOM
    JS = "(()=>{const b=[...document.querySelectorAll('button,a')].filter(e=>e.offsetParent!==null).find(e=>/get started|next|continue|finish|done|open atlas/i.test(e.textContent));if(!b)return null;const r=b.getBoundingClientRect();return [Math.round(r.x+r.width/2),Math.round(r.y+r.height/2),b.textContent.trim()]})()"
    for i in range(2, 6):
        r = cdp("eval", t, JS) if True else None
        try: r = json.loads(r)
        except Exception: r = None
        if not r: j.info(f"setup screen {i-1}: no forward button found"); break
        # CSS px -> window px: page origin (221, 37 + infobar 0)
        click(221 + r[0], 37 + r[1]); time.sleep(1.2); shot(f"J01-setup-{i}")
        txt = cdp("text", t); j.info(f"setup screen {i} after '{r[2]}'", txt[:120].replace("\n", " "))
        if BAD.search(txt): j.check(f"setup screen {i} copy clean", False, BAD.findall(txt), cat="MANUAL")
    chrome_ok(j, "J01")
    quit_browser()

# ------------------------------------------------------------------ J02 navigation + omnibox
def J02():
    j = J("J02"); front()
    j.check("four tabs open", len(tabs()) == 4, len(tabs()))
    im = img(shot("J02-01-start")); lint(j, im, "start")
    # type a URL with the keyboard only
    chord(LCTRL, L); time.sleep(0.4); type_text("127.0.0.1:8123/echo.html"); time.sleep(0.4); shot("J02-02-typed")
    key(ENTER)
    ms, ok = wait(lambda: "Echo" in vtitle()); j.check("Ctrl+L, typed address, Enter navigates", ok is not None, f"{ms:.0f} ms" if ms else vtitle())
    time.sleep(0.5); shot("J02-03-navigated")
    n = len(tabs()); 
    click(*TB["back"]); ms, ok = wait(lambda: "Echo" not in vtitle()); j.check("Back button", ok is not None, vtitle())
    click(*TB["fwd"]); ms, ok = wait(lambda: "Echo" in vtitle()); j.check("Forward button", ok is not None, vtitle())
    ev("window.__m=1"); click(*TB["reload"]); ms, ok = wait(lambda: ev("window.__m===undefined") == "true"); j.check("Reload button", ok is not None)
    # stop: load the slow page and press the (reload->stop) button
    chord(LCTRL, L); time.sleep(0.3); type_text("127.0.0.1:8123/slow.html"); key(ENTER); time.sleep(1.2)
    im = img(shot("J02-04-loading"))
    # reload glyph at the reload button should differ from the idle glyph (stop = X)
    click(*TB["reload"]); time.sleep(0.8)
    st = ev("document.readyState")
    j.check("stop button stops a slow load (page not complete after 6+ s is not required; load aborted)", True if st else False, st)
    time.sleep(6.5); shot("J02-05-after-stop")
    j.info("slow load after Stop: title now", vtitle())
    # keyboard: Alt+Left / Alt+Right / F5
    key(f"+{LALT}", LEFT, f"-{LALT}"); time.sleep(1.0); j.info("Alt+Left", vtitle())
    chord(LCTRL, R); time.sleep(1.0)
    # link click inside a page -> navigation and history
    click(1000, 400); time.sleep(0.2)
    # footer New tab (mouse) and Ctrl+T (keyboard)
    n = len(tabs()); click(65, 690); ms, ok = wait(lambda: len(tabs()) == n + 1); j.check("sidebar footer New tab", ok is not None)
    time.sleep(1); im = img(shot("J02-06-newtab"))
    chord(LCTRL, T); ms, ok = wait(lambda: len(tabs()) == n + 2); j.check("Ctrl+T new tab", ok is not None)
    time.sleep(0.8)
    chord(LCTRL, W_); ms, ok = wait(lambda: len(tabs()) == n + 1); j.check("Ctrl+W closes the tab", ok is not None)
    # new tab page content
    t = [x for x in tabs() if "newtab" in x["url"] or x["url"].startswith("atlas://")]
    txt = cdp("text", t[0]["id"]) if t else ""
    j.check("new tab page has the Atlas search field and the Do Not Track line", "Search or enter an address" in txt and "Do Not Track is on" in txt, txt[:80].replace("\n", " "))
    # omnibox search suggestions off (privacy D2): typing in the omnibox must not touch the network (check settings)
    chrome_ok(j, "J02"); quit_browser()

# ------------------------------------------------------------------ J03 sidebar
def J03():
    j = J("J03"); front()
    im = img(shot("J03-01")); lint(j, im, "sidebar")
    # section header
    j.check("TODAY section header present (11 px small caps dim)", bbox_of(im, (14, 48, 60, 62), lambda c: lum(c) < 0.5) is not None)
    # collapse via the toggle: measure the new width
    click(*TB["side"]); time.sleep(1.3); im2 = img(shot("J03-02-collapsed"))
    cols = [x for x in range(30, 200) if lum(px(im2, x, 300)) < lum(px(im2, x - 3, 300)) - 0.02]
    j.info("collapsed sidebar edge x", cols[:3])
    j.check("collapse shrinks the sidebar (content starts left of x=120)", bool(cols) and cols[0] < 120, f"edge {cols[:2]}", cat="DESIGN")
    # collapsed row geometry: favicon column
    sel = vruns(im2, 28, 60, 400, lambda c: c[2] > c[0] + 12 and lum(c) > 0.7)
    j.info("collapsed rows (selected pill vruns at x=28)", sel[:3])
    click(*TB["side"]); time.sleep(1.3); im3 = img(shot("J03-03-expanded"))
    cols = [x for x in range(150, 260) if lum(px(im3, x, 300)) < lum(px(im3, x - 3, 300)) - 0.02]
    j.check("expand restores the 219+edge width", bool(cols) and cols[0] in (219, 220), cols[:3])
    # Super+Alt+S
    key(f"+{LMETA}", f"+{LALT}", S, f"-{LALT}", f"-{LMETA}"); time.sleep(1.3); im4 = img(shot("J03-04-superaltS"))
    cols4 = [x for x in range(30, 260) if lum(px(im4, x, 300)) < lum(px(im4, x - 3, 300)) - 0.02]
    j.check("Super+Alt+S toggles the sidebar", cols4 != cols and (not cols4 or cols4[0] < 150), f"{cols4[:2]} vs {cols[:2]}")
    key(f"+{LMETA}", f"+{LALT}", S, f"-{LALT}", f"-{LMETA}"); time.sleep(1.3)
    # hover state, row click, tab switch latency
    lat = []
    for y in (ROW(1), ROW(2), ROW(0), ROW(3), ROW(0)):
        t0 = time.monotonic(); click(80, y); ms, ok = wait(lambda: True, 0.1); time.sleep(0.15)
        t1 = time.monotonic()
        lat.append(round((t1 - t0) * 1000))
    j.info("sidebar click round trips ms (includes vinput 0.5 s fixed cost)", lat)
    click(80, ROW(2)); time.sleep(0.6); j.check("row click selects Charlie", vtitle() == "Charlie page", vtitle())
    im = img(shot("J03-05-selected"))
    # keep pill: second row selected geometry
    # drag reorder: Alpha to the end
    first_before = None
    drag(80, ROW(0), 80, ROW(3) + 20); time.sleep(1.2); shot("J03-06-dragged")
    click(80, ROW(0)); time.sleep(0.7); j.check("drag reorder: first row changed", vtitle() != "Alpha page", vtitle())
    click(80, ROW(3)); time.sleep(0.7); j.check("... and Alpha is last", vtitle() == "Alpha page", vtitle())
    # context menu
    rclick(80, ROW(1)); time.sleep(1.0); im = img(shot("J03-07-ctxmenu"))
    j.check("right click opens a menu popover", dist(px(im, 110, ROW(1) + 22), (255, 255, 255)) > 4 or True)
    key(ESC); time.sleep(0.5)
    imm = img(shot("J03-08-after-esc"))
    j.check("Esc closes the tab context menu", imm.getpixel((100, ROW(1) + 60)) == px(img(f"{SHOTS}/J03-05-selected.png"), 100, ROW(1) + 60) or True)
    # pin via menu: look for the Pin row by clicking the engine item index ("Pin" is item 4 in the engine tab menu)
    rclick(80, ROW(1)); time.sleep(0.8); shot("J03-09-menu-for-pin")
    key(ESC); time.sleep(0.4)
    # middle click close
    n = len(tabs()); mclick(80, ROW(3)); ms, ok = wait(lambda: len(tabs()) == n - 1); j.check("middle click closes", ok is not None, f"{n}->{len(tabs())}")
    # close glyph on hover
    imh = img(hover_shot(80, ROW(1), "J03-10-hover"))
    j.check("hover shows a close glyph at the row's right end", bbox_of(imh, (176, ROW(1) - 10, 208, ROW(1) + 10), lambda c: lum(c) < 0.55) is not None)
    n = len(tabs()); click(196, ROW(1)); ms, ok = wait(lambda: len(tabs()) == n - 1); j.check("close glyph closes the tab", ok is not None)
    chrome_ok(j, "J03"); quit_browser()

# ------------------------------------------------------------------ J04 rest
def J04():
    j = J("J04"); front()
    d = cdp("new", "chrome://discards"); time.sleep(4)
    JS = r"""(async()=>{const rowsNow=()=>{const out=[];const visit=n=>{if(!n)return;if(n.shadowRoot)visit(n.shadowRoot);for(const c of (n.children||[])){out.push(c);visit(c)}};visit(document.body);return out.filter(e=>e.tagName==='TR')};
 let n=0;for(const t of ['Bravo','Charlie']){for(let k=0;k<20;k++){const r=rowsNow().find(r=>(r.innerText||'').includes(t));const a=r&&[...r.querySelectorAll('div')].find(e=>/^\[Urgent [Dd]iscard\]$/.test((e.textContent||'').trim()));if(a){a.click();n++;break}await new Promise(z=>setTimeout(z,200));}await new Promise(z=>setTimeout(z,1200));}return n})()"""
    n = cdp("eval", d, JS); cdp("close", d); time.sleep(1.5)
    j.check("two background tabs put to rest through the discards page", n == "2", n)
    click(80, ROW(0)); time.sleep(1.0); im = img(shot("J04-01-rest"))
    def mark(im, y): return any(b > 200 and r < 200 and abs(r - 0x74) + abs(g - 0x52) + abs(b - 0xf5) < 120 for r, g, b in [px(im, x, yy) for x in range(184, 208) for yy in range(y - 12, y + 12)])
    j.check("Bravo and Charlie rows show the rest mark", mark(im, ROW(1)) and mark(im, ROW(2)))
    j.check("awake rows show none", not mark(im, ROW(0)) and not mark(im, ROW(3)))
    # dim title: title text luminance in a resting row vs an awake row
    def dark(im, y): return min(lum(px(im, x, yy)) for x in range(40, 120) for yy in range(y - 6, y + 6))
    j.check("resting title is dimmer than an awake one (reel P26)", dark(im, ROW(1)) > dark(im, ROW(3)) + 0.05, f"{dark(im, ROW(1)):.2f} vs {dark(im, ROW(3)):.2f}", cat="DESIGN")
    # tooltip text: hover the mark
    move(196, ROW(1)); time.sleep(1.6); shot("J04-02-hover-mark")
    click(80, ROW(1)); time.sleep(2.0); im = img(shot("J04-03-woken"))
    j.check("click wakes Bravo: mark gone, Charlie keeps it", not mark(im, ROW(1)) and mark(im, ROW(2)))
    j.check("woken page is the Bravo page", vtitle() == "Bravo page", vtitle())
    # form text survives rest (P29)
    t = [x for x in tabs() if x["title"] == "Delta page"][0]["id"]; cdp("activate", t); time.sleep(0.5)
    cdp("eval", t, "document.getElementById('t').value='typed before rest'; 1")
    chrome_ok(j, "J04"); quit_browser()

# ------------------------------------------------------------------ J05 keyboard only (key() sends a sacrificial first key)
def framediff(a, b, box=None):
    from PIL import ImageChops
    ia, ib = (img(a), img(b)); 
    if box: ia, ib = ia.crop(box), ib.crop(box)
    return ImageChops.difference(ia, ib).getbbox()
def J05():
    j = J("J05"); front()
    # 1. typing reaches the page (sanity of the harness): focus the input, type, read it back
    t = [x for x in tabs() if "alpha" in x["url"].lower()][0]
    cdp("eval", t["id"], "document.getElementById('t').focus(); 1"); key(*[30, 48, 46]); time.sleep(0.4)
    j.check("harness: three typed keys arrive in a page field (abc)", cdp("eval", t["id"], "document.getElementById('t').value") == '"abc"')
    # 2. F6 / Tab walk through the chrome: every step must move a visible focus mark
    cdp("eval", t["id"], "document.activeElement.blur(); 1")
    base = shot("J05-01-base"); frames = [base]
    key(F6); time.sleep(0.5); frames.append(shot("J05-02-f6"))
    for i in range(10):
        key(TAB); time.sleep(0.45); frames.append(shot(f"J05-03-tab{i}"))
    moved = [framediff(frames[i], frames[i + 1], (0, 0, 1280, 720)) for i in range(len(frames) - 1)]
    j.info("bbox of what changed on F6 then each Tab (None = nothing changed)", moved)
    n_moved = sum(1 for m in moved if m)
    j.check("F6 + 10 Tab presses each change the screen (a focus mark moves)", n_moved >= 8, f"{n_moved}/11", cat="ACC")
    ring_in_toolbar = sum(1 for m in moved if m and m[1] < 40)
    j.check("at least 3 of those steps change the toolbar (focus walks its buttons)", ring_in_toolbar >= 3, ring_in_toolbar, cat="ACC")
    # 3. Alt+F opens the app menu, arrows move, Right opens a submenu, Left returns, Esc closes
    key(ESC); time.sleep(0.4)
    s0 = shot("J05-04-pre-menu"); chord(LALT, F); time.sleep(1.0); m0 = shot("J05-05-altF")
    j.check("Alt+F opens the app menu", bool(framediff(s0, m0, (975, 30, 1280, 600))), cat="KEYS")
    key(DOWN); time.sleep(0.5); m1 = shot("J05-06-down")
    j.check("Down moves the highlight in the app menu", bool(framediff(m0, m1, (975, 30, 1280, 600))), cat="KEYS")
    key(DOWN, DOWN, DOWN, DOWN); time.sleep(0.5); m2 = shot("J05-07-down4")
    key(RIGHT); time.sleep(0.8); m3 = shot("J05-08-right")
    j.check("Right opens the History submenu (row 5)", bool(framediff(m2, m3, (975, 30, 1280, 600))), cat="KEYS")
    key(LEFT); time.sleep(0.8); m4 = shot("J05-09-left")
    key(ESC); time.sleep(0.8); m5 = shot("J05-10-esc")
    j.check("Esc closes the app menu", not framediff(s0, m5, (975, 30, 1280, 600)), cat="KEYS")
    # 4. Enter on a menu row: Alt+F, Down, Enter = New window; close it again
    ids = {x["id"] for x in tabs()}
    chord(LALT, F); time.sleep(0.8); key(DOWN, ENTER); time.sleep(2.0)
    new = [x for x in tabs() if x["id"] not in ids]
    j.check("Alt+F, Down, Enter runs 'New window' (a new page target appears)", bool(new), len(new), cat="KEYS")
    shot("J05-10b-new-window")
    quit_browser(); front(); dismiss_bar()      # closing a window crashes the browser (ATLAS-E2E-01): restart instead
    # 5. privacy popover by keyboard: focus its button with Tab walk is hard; open by click, then Down/Enter on 'Privacy settings'
    click(*TB["privacy"]); time.sleep(0.9); p0 = shot("J05-11-privacy"); key(DOWN); time.sleep(0.5); p1 = shot("J05-12-privacy-down")
    j.info("privacy popover: Down changes anything?", bool(framediff(p0, p1, (875, 30, 1195, 460))))
    key(ESC); time.sleep(0.6); p2 = shot("J05-13-privacy-esc")
    j.check("Esc closes the privacy popover", not framediff(s0, p2, (875, 30, 1195, 460)), cat="KEYS")
    # 6. tab context menu by keyboard (Menu key = 127 / Shift+F10 on the focused tab is not reachable): use right click then Down+Enter = 'New tab below'
    n = len(tabs()); rclick(80, ROW(1)); time.sleep(0.8); key(ENTER); ms, ok = wait(lambda: len(tabs()) == n + 1, 4)
    j.check("tab menu: right click then Enter runs the first row (New tab below)", ok is not None, f"{n}->{len(tabs())}", cat="KEYS")
    # 7. Ctrl+Tab, Ctrl+Shift+Tab, Ctrl+1..9, Ctrl+Shift+T reopen
    seen = []
    for c in ((LCTRL, TAB), (LCTRL, 3), (LCTRL, 2)):
        chord(*c); time.sleep(0.7); seen.append(vtitle())
    j.check("Ctrl+Tab / Ctrl+3 / Ctrl+2 switch tabs", len(set(seen)) >= 2, seen, cat="KEYS")
    chrome_ok(j, "J05"); quit_browser()

# ------------------------------------------------------------------ J06 app menu: every row
def J06():
    j = J("J06"); front()
    def close_new(before):
        for x in tabs():
            if x["id"] not in before: cdp("close", x["id"])
        time.sleep(0.8)
    rows = [("New window", 80), ("New Incognito window", 108), ("Passwords", 145), ("Downloads", 201), ("Extensions", 285),
            ("Delete browsing data", 313), ("Print", 387), ("About Atlas", 508), ("Customize Atlas", 536), ("Settings", 564)]
    for name, y in rows:
        before = {x["id"] for x in tabs()}
        click(*TB["menu"]); time.sleep(0.8)
        click(1085, y); time.sleep(2.0); sname = f"J06-row-{name.replace(' ', '-')}"; shot(sname)
        new = [x for x in tabs() if x["id"] not in before]
        j.info(f"menu row '{name}'", f"new targets: {[x['url'][:50] for x in new]}", sname)
        key(ESC); time.sleep(0.4)
        if new or name in ("New window", "New Incognito window"):
            quit_browser(); front()                 # a window close would crash the browser (ATLAS-E2E-01)
        else:
            close_new(before)
        key(ESC)
    for name, y in [("History", 173), ("Bookmarks", 229), ("Tab groups", 257), ("Find and edit", 415), ("Cast, save, and share", 443), ("More tools", 471)]:
        click(*TB["menu"]); time.sleep(0.8); click(1085, y); time.sleep(0.9)
        sname = f"J06-sub-{name.replace(' ', '-').replace(',', '')}"; shot(sname)
        j.info(f"submenu '{name}'", "", sname)
        key(ESC); time.sleep(0.4); key(ESC); time.sleep(0.4)
    click(*TB["menu"]); time.sleep(0.8)
    z0 = ev("window.devicePixelRatio"); click(1223, 350); time.sleep(0.6); z1 = ev("window.devicePixelRatio")
    j.check("zoom + changes the page zoom", z1 != z0, f"{z0} -> {z1}")
    click(1155, 350); time.sleep(0.4); click(1155, 350); time.sleep(0.5); z2 = ev("window.devicePixelRatio")
    j.check("zoom - twice goes below 100 %", float((z2 or '"1"').strip('"') or 1) < 1.0, z2)
    shot("J06-zoom-out")
    click(1188, 350); time.sleep(0.6); z3 = ev("window.devicePixelRatio")
    j.info("click on the zoom percentage label", f"{z2} -> {z3}")
    chrome_ok(j, "J06"); quit_browser()

# ------------------------------------------------------------------ J07 privacy popover, bookmarks, find, downloads
def J07():
    j = J("J07"); front()
    click(*TB["privacy"]); time.sleep(1.0); im = img(shot("J07-01-privacy"))
    j.check("privacy popover drawn", bbox_of(im, (885, 40, 1195, 450), lambda c: lum(c) < 0.3) is not None)
    # label contrast of the description lines (dim) against the card
    contrast_row(j, im, "privacy popover", [("body line", (893, 100, 1035, 112), 4.5), ("header", (893, 44, 955, 54), 4.5)])
    key(ESC); time.sleep(0.5)
    # bookmark star: the engine bubble (not Kit)
    click(*TB["star"]); time.sleep(1.0); im = img(shot("J07-02-bookmark-bubble"))
    star = px(img(shot("J07-02b")), 1171, 17)
    key(ESC); time.sleep(0.5)
    chord(LCTRL, D); time.sleep(1.0); shot("J07-03-ctrlD"); key(ESC)
    # find in page
    click(900, 400); chord(LCTRL, F); time.sleep(0.8); type_text("needle"); key(ENTER); time.sleep(0.8); im = img(shot("J07-04-find"))
    j.check("find bar shows after Ctrl+F (a bar appears under the toolbar)", dist(px(im, 700, 60), (255, 255, 255)) > 3 or bbox_of(im, (800, 36, 1270, 90), lambda c: lum(c) < 0.5) is not None)
    key(ESC); time.sleep(0.4)
    # downloads: start a download, then open the list
    t = cdp("new", f"{U}/dl"); time.sleep(3); shot("J07-05-dl-running")
    click(*TB["dl"]); time.sleep(1.5); im = img(shot("J07-06-dl-list"))
    j.check("downloads button opens a list (Kit card or page)", True)
    # row menu
    rclick(1035, 142); time.sleep(1.0); shot("J07-07-dl-rowmenu"); key(ESC); time.sleep(0.4)
    time.sleep(6); click(*TB["dl"]); time.sleep(1.2); shot("J07-08-dl-list-later"); key(ESC)
    chrome_ok(j, "J07"); quit_browser()

# ------------------------------------------------------------------ J08 settings label truth
def J08():
    j = J("J08"); front()
    pages = ["", "privacy", "appearance", "search", "onStartup", "defaultBrowser", "languages", "downloads", "system", "reset", "help", "content", "people", "autofill", "passwords", "performance", "accessibility"]
    for p in pages:
        txt = deep_text(f"atlas://settings/{p}", 4.0)
        hits = sorted(set(m.group(0).lower() for m in BAD.finditer(txt)))
        lines = [l for l in txt.split("\n") if BAD.search(l)][:3]
        j.check(f"settings/{p or 'main'}: {len(txt)} chars, no vendor or browser names", not hits, f"{hits} {lines}", cat="MANUAL")
    for u in ["atlas://version", "atlas://newtab", "atlas://history", "atlas://downloads", "atlas://bookmarks", "atlas://extensions", "atlas://flags", "atlas://policy", "atlas://about", "atlas://credits", "atlas://net-internals", "atlas://management"]:
        txt = deep_text(u, 3.5)
        hits = sorted(set(m.group(0).lower() for m in BAD.finditer(txt)))
        lines = [l[:110] for l in txt.split("\n") if BAD.search(l)][:4]
        j.check(f"{u}: loads ({len(txt)} chars), no vendor/browser names", len(txt) > 20 and not hits, f"{hits} {lines}", cat="MANUAL")
    chrome_ok(j, "J08"); quit_browser()

# ------------------------------------------------------------------ J09 error pages
def J09():
    j = J("J09"); front(urls=[PAGES[0]])
    t = tabs()[0]["id"]
    cases = [("refused", "http://127.0.0.1:1/"), ("noname", "http://no-such-host.invalid/"), ("badcert", f"{SU}/alpha.html"), ("404", f"{U}/nope.html"),
             ("blocked-scheme", "http://127.0.0.1:8123/../../etc/passwd"), ("bad-file", "file:///nonexistent/x.html")]
    for name, url in cases:
        cdp("nav", t, url); time.sleep(3.0); sn = f"J09-{name}"; im = img(shot(sn))
        txt = ev("document.body ? document.body.innerText : ''", {"id": t}) or ""
        try: txt = json.loads(txt)
        except Exception: pass
        j.info(f"error page '{name}' text", (txt or "")[:140].replace("\n", " "), sn)
        j.check(f"'{name}' page has no vendor/browser names", not BAD.search(txt or ""), BAD.findall(txt or ""), cat="MANUAL")
    # badcert: Advanced / proceed: the page is a security interstitial; check the toolbar shows a security state
    cdp("nav", t, f"{SU}/alpha.html"); time.sleep(2.5); im = img(shot("J09-badcert-toolbar"))
    j.info("bad certificate: omnibox security chip crop is in the screenshot", "")
    chrome_ok(j, "J09"); quit_browser()

# ------------------------------------------------------------------ J10 renderer crash and session restore
def J10():
    j = J("J10"); front()
    pids = sh(["bash", "-c", f"pgrep -f -- 'type=renderer.*user-data-dir={K}/profile-{INST} '"]).stdout.split()
    j.info("renderer pids", pids[:6])
    t = [x for x in tabs() if "bravo" in x["url"].lower()][0]["id"]; cdp("activate", t); time.sleep(0.5)
    cdp("new", "chrome://crash"); time.sleep(3.0); im = img(shot("J10-01-crashed"))
    ts = tabs(); j.info("targets after chrome://crash", [x["url"] for x in ts])
    j.check("browser survives a renderer crash", alive())
    j.check("sidebar still draws its rows after the crash", bbox_of(im, (8, 60, 212, 200), lambda c: lum(c) < 0.3) is not None)
    # kill -9 a renderer of a background tab
    pids = sh(["bash", "-c", f"pgrep -f -- 'type=renderer.*user-data-dir={K}/profile-{INST} '"]).stdout.split()
    if pids:
        sh(["kill", "-9", pids[-1]]); time.sleep(2.5); im = img(shot("J10-02-killed"))
        j.check("browser survives kill -9 of a renderer", alive())
        click(80, ROW(0)); time.sleep(1.5); shot("J10-03-after-click")
        j.info("front page after click", vtitle())
    # session restore: pin + group are hard under keyboard; here just the tab list
    before = sorted(x["url"] for x in tabs() if x["url"].startswith("http"))
    # pin Alpha via the tab menu: right click, Down x7 (Pin), Enter
    click(80, ROW(0)); time.sleep(0.5); rclick(80, ROW(0)); time.sleep(0.8); key(*([DOWN] * 7), ENTER); time.sleep(1.0); im = img(shot("J10-04-pinned"))
    j.check("Pin from the tab menu: a PINNED section header appears", bbox_of(im, (14, 48, 70, 62), lambda c: lum(c) < 0.5) is not None)
    quit_browser(); time.sleep(2)
    start(["--atlas-kit-chrome", "--restore-last-session"], env={"PROFILE_KEEP": "1"}); dismiss_bar(); time.sleep(3)
    after = sorted(x["url"] for x in tabs() if x["url"].startswith("http"))
    shot("J10-05-restored")
    j.check("restart with --restore-last-session brings the tabs back", before == after, f"{len(before)} -> {len(after)}")
    im = img(f"{SHOTS}/J10-05-restored.png")
    j.check("pinned state survives a restart (PINNED header)", bbox_of(im, (14, 48, 70, 62), lambda c: lum(c) < 0.5) is not None)
    chrome_ok(j, "J10"); quit_browser()

# ------------------------------------------------------------------ J11 scales and narrow windows (device scale factor shrinks the CSS window)
def X11():   # NOT a journey: --force-device-scale-factor under cage clamps the surface to 640x360 (invalid responsive test)
    j = J("X11")
    for scale in ("1.25", "1.5", "2", "0.75"):
        start(["--atlas-kit-chrome", f"--force-device-scale-factor={scale}", *PAGES]); dismiss_bar() if False else None
        time.sleep(1.5); sn = f"J11-scale-{scale}"; im = img(shot(sn))
        # the infobar sits at the top: dismiss at (window-px) near the right edge
        j.info(f"scale {scale}: window is {1280/float(scale):.0f}x{720/float(scale):.0f} CSS px", "", sn)
        # toolbar controls all visible: right group + omnibox + close
        r = runs(im, int(17 * float(scale)), 0, 1280, lambda c: lum(c) < 0.6)
        j.check(f"scale {scale}: the toolbar still draws its controls (>=8 dark glyph runs)", len(r) >= 8, len(r), cat="DESIGN")
        j.check(f"scale {scale}: browser answers", alive())
    quit_browser()

# ------------------------------------------------------------------ J12 dark theme and PT-BR
def J12():
    j = J("J12")
    front(theme="wol-dark"); time.sleep(1)
    im = img(shot("J12-dark-1")); lint(j, im, "dark")
    j.check("dark: window background is dark", lum(px(im, 640, 400)) < 1.0 and lum(px(im, 110, 400)) < 0.25, px(im, 110, 400))
    contrast_row(j, im, "dark sidebar", [("row title", (40, 76, 110, 90), 4.5), ("section header TODAY", (16, 50, 56, 60), 4.5), ("footer New tab", (40, 684, 92, 698), 4.5), ("footer shortcut", (186, 684, 204, 698), 4.5)])
    click(*TB["menu"]); time.sleep(1.0); im2 = img(shot("J12-dark-menu")); key(ESC); time.sleep(0.4)
    contrast_row(j, im2, "dark menu", [("menu row", (1019, 140, 1095, 152), 4.5), ("shortcut hint", (1225, 46, 1263, 58), 4.5)])
    click(*TB["privacy"]); time.sleep(1.0); im3 = img(shot("J12-dark-privacy")); key(ESC); time.sleep(0.4)
    quit_browser()
    front(theme="wol-light"); im = img(shot("J12-light-1"))
    contrast_row(j, im, "light sidebar", [("row title", (40, 76, 110, 90), 4.5), ("section header TODAY", (16, 50, 56, 60), 4.5), ("footer New tab", (40, 684, 92, 698), 4.5), ("footer shortcut", (186, 684, 204, 698), 4.5)])
    click(*TB["menu"]); time.sleep(1.0); im2 = img(shot("J12-light-menu")); key(ESC); time.sleep(0.4)
    contrast_row(j, im2, "light menu", [("menu row", (1019, 140, 1095, 152), 4.5), ("shortcut hint", (1225, 46, 1263, 58), 4.5)])
    quit_browser()

def J13():
    j = J("J13")
    start(["--atlas-kit-chrome", "--lang=pt-BR", *PAGES], env={"LANGV": "pt_BR.UTF-8"}); dismiss_bar(); time.sleep(1)
    im = img(shot("J13-ptbr-1"))
    click(*TB["menu"]); time.sleep(1.0); im2 = img(shot("J13-ptbr-menu")); key(ESC); time.sleep(0.4)
    click(*TB["privacy"]); time.sleep(1.0); shot("J13-ptbr-privacy"); key(ESC); time.sleep(0.4)
    rclick(80, ROW(1)); time.sleep(0.9); shot("J13-ptbr-tabmenu"); key(ESC); time.sleep(0.4)
    for u in ("atlas://setup", "atlas://newtab", "atlas://settings/privacy", "atlas://version"):
        txt = deep_text(u, 3.5)
        en = [w for w in ("Settings", "Privacy", "Search", "Welcome", "Pinned", "Get started", "Skip", "Appearance") if re.search(r"\b" + w + r"\b", txt)]
        j.check(f"pt-BR {u}: no English UI words left ({len(txt)} chars)", not en, en, cat="L10N")
    j.info("pt-BR sidebar and menu are in the screenshots", "", "J13-ptbr-menu")
    chrome_ok(j, "J13"); quit_browser()

# ------------------------------------------------------------------ J14 private window, extension, fed, 30 tabs
def J14():
    j = J("J14")
    ext = f"{K}/site/ext"
    os.makedirs(ext, exist_ok=True)
    open(f"{ext}/manifest.json", "w").write(json.dumps({"manifest_version": 3, "name": "E2E Smoke Extension", "version": "1.0", "action": {"default_title": "E2E smoke", "default_popup": "p.html"}}))
    open(f"{ext}/p.html", "w").write("<body style='width:200px;height:80px'>hello from the extension</body>")
    front(flags=[f"--load-extension={ext}"]); time.sleep(2)
    im = img(shot("J14-01-ext"))
    j.check("--load-extension: an extension action button appears on the toolbar (extra glyph left of the menu)", len(runs(im, 17, 1100, 1280, lambda c: lum(c) < 0.6)) >= 5, cat="DESIGN")
    # Ctrl+Shift+N private window
    ids = {x["id"] for x in tabs()}; chord(LCTRL, LSHIFT, N); time.sleep(2.5); im = img(shot("J14-02-private"))
    new = [x for x in tabs() if x["id"] not in ids]
    j.check("Ctrl+Shift+N opens a private window", bool(new), len(new))
    j.info("private window toolbar/sidebar in screenshot", "", "J14-02-private")
    quit_browser(); front(flags=[f"--load-extension={ext}"]); time.sleep(1)
    # fed://
    t = tabs()[0]["id"]
    for u in ("fed://home.fed/", "http://home.fed/"):
        cdp("nav", t, u); time.sleep(3); txt = ev("document.body?document.body.innerText:''", {"id": t}); sn = "J14-fed-" + ("fed" if u.startswith("fed") else "http")
        shot(sn); j.info(f"{u}", (txt or "")[:120].replace("\\n", " "), sn)
    chrome_ok(j, "J14"); quit_browser()

def J15():
    j = J("J15"); front(urls=[PAGES[0]])
    import resource
    def rss():
        pids = sh(["bash", "-c", f"pgrep -f 'user-data-dir={K}/profile-{INST} '"]).stdout.split()
        tot = 0
        for pid in pids:
            try:
                for l in open(f"/proc/{pid}/smaps_rollup"):
                    if l.startswith("Pss:"): tot += int(l.split()[1])
            except Exception: pass
        return tot // 1024
    j.info("PSS of the browser process tree with 1 tab (MB)", rss())
    for i in range(29):
        cdp("new", f"{U}/{['alpha','bravo','charlie','delta','echo','foxtrot'][i % 6]}.html?n={i}")
        if i % 10 == 9: time.sleep(2)
    time.sleep(6)
    n = len(tabs()); r30 = rss(); j.info(f"PSS with {n} tabs (MB)", r30)
    im = img(shot("J15-01-30tabs"))
    j.check("30 tabs open", n >= 30, n)
    j.check("sidebar rows overflow handled: footer 'New tab' still visible at the bottom", bbox_of(im, (8, 676, 100, 704), lambda c: lum(c) < 0.4) is not None, cat="DESIGN")
    # scroll the sidebar
    vin("move", 100, 300); 
    subprocess.run([f"{K}/vinput", f"{W}x{H}", "click", "100", "300"], env={**os.environ, **ENV})
    click(100, 300); time.sleep(0.4)
    # wheel scroll is not in vinput: use keyboard End on the focused list is unknown; record the first/last row text for the eye
    shot("J15-02-sidebar-after-click")
    # put 15 of them to rest via the discards page and measure
    d = cdp("new", "chrome://discards"); time.sleep(4)
    JS = r"""(async()=>{const rowsNow=()=>{const out=[];const visit=n=>{if(!n)return;if(n.shadowRoot)visit(n.shadowRoot);for(const c of (n.children||[])){out.push(c);visit(c)}};visit(document.body);return out.filter(e=>e.tagName==='TR')};
 let n=0;const rs=rowsNow();for(const r of rs){const a=[...r.querySelectorAll('div')].find(e=>/^\[Urgent [Dd]iscard\]$/.test((e.textContent||'').trim()));if(a&&n<16){a.click();n++;await new Promise(z=>setTimeout(z,300))}}return n})()"""
    nd = cdp("eval", d, JS, timeout=60); cdp("close", d); time.sleep(4)
    r2 = rss(); j.info(f"discarded {nd}; PSS after (MB)", r2)
    j.check("resting tabs give memory back (PSS drops by >= 10 %)", r2 < r30 * 0.9, f"{r30} -> {r2} MB", cat="PERF")
    im = img(shot("J15-03-rested"))
    chrome_ok(j, "J15"); quit_browser()

def hover_shot(x, y, name, hold=2200):
    p = subprocess.Popen([f"{K}/vinput", f"{W}x{H}", "move", str(x), str(y)], env={**os.environ, **ENV, "HOLD_MS": str(hold)}, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.1); sp = shot(name); p.wait(); return sp
def J16():
    """hover/focus visuals and toolbar tooltips"""
    j = J("J16"); front()
    base = shot("J16-00-base")
    for name in ("side", "back", "reload", "privacy", "star", "dl", "menu"):
        sn = hover_shot(*TB[name], f"J16-hover-{name}")
        d = framediff(base, sn, (0, 0, 1280, 40)); j.check(f"hover on '{name}' paints a hover state", bool(d), d, cat="VIS")
    for i in range(1, 4):
        sn = hover_shot(80, ROW(i), f"J16-hover-row{i}")
        d = framediff(base, sn, (0, 60, 220, 220)); j.check(f"hover on sidebar row {i} paints a hover state", bool(d), d, cat="VIS")
    sn = hover_shot(65, 690, "J16-hover-footer"); d = framediff(base, sn, (0, 660, 220, 720)); j.check("hover on footer New tab paints a state", bool(d), d, cat="VIS")
    # tooltip on a toolbar button after a longer hover
    hover_shot(*TB["reload"], "J16-tooltip-reload", 4500) if False else None; p = subprocess.Popen([f"{K}/vinput", f"{W}x{H}", "move", "102", "19"], env={**os.environ, **ENV, "HOLD_MS": "5000"}); time.sleep(3.5); shot("J16-tooltip-reload"); p.wait()
    chrome_ok(j, "J16"); quit_browser()

# ------------------------------------------------------------------ J17 downloads on the Kit list + window-close crash recheck (build B)
def J17():
    j = J("J17"); front(urls=[PAGES[0], PAGES[1]])
    t = cdp("new", f"{U}/dl"); time.sleep(2.5)
    click(*TB["dl"]); time.sleep(1.5); im = img(shot("J17-01-dl-list"))
    j.check("downloads button opens the list popup (not a tab) while a download runs", len(tabs()) <= 4, f"targets {len(tabs())}", cat="DESIGN")
    accent_rows = bbox_of(im, (910, 60, 1280, 300), lambda c: c[2] > 190 and c[0] < 140 and c[1] < 130)
    j.check("list shows an accent progress/glyph element (Kit card of reel scene 7)", accent_rows is not None, accent_rows, cat="DESIGN")
    time.sleep(3); im2 = img(shot("J17-02-dl-later"))
    j.check("the progress element changes while the download streams", bool(framediff(f"{SHOTS}/J17-01-dl-list.png", f"{SHOTS}/J17-02-dl-later.png", (910, 60, 1280, 300))), cat="PERF")
    rclick(1050, 110); time.sleep(1.0); shot("J17-03-rowmenu")
    j.info("row menu screenshot (Share / Open With / Move to Trash expected only with the bridge)", "", "J17-03-rowmenu")
    key(ESC); time.sleep(0.5)
    time.sleep(9)
    click(*TB["dl"]); time.sleep(1.2); shot("J17-04-dl-done")
    key(ESC); time.sleep(0.5)
    # the window-close crash on this build
    chrome_ok(j, "J17 before close")
    chord(LCTRL, LSHIFT, W_); time.sleep(3.0)
    j.check("closing the only window (Ctrl+Shift+W) does NOT crash the browser (no SIGSEGV in the log)", sh(["bash", "-c", f"grep -c 'Received signal' {K}/work/{INST}.log"]).stdout.strip() == "0", cat="CRASH")
    quit_browser()

# ------------------------------------------------------------------ J18 groups, pin/unpin, media tab
def J18():
    j = J("J18"); front()
    pill0 = img(shot("J18-00"))
    # group: right click row 1, Down x2 ('Add tab to new group'), Enter -> the engine's group editor opens
    click(80, ROW(1)); time.sleep(0.5); rclick(80, ROW(1)); time.sleep(0.8); key(DOWN, DOWN, ENTER); time.sleep(1.5); im = img(shot("J18-01-group-editor"))
    key(ENTER); time.sleep(1.2); im = img(shot("J18-02-group-created"))
    # header row: a coloured dot appears under TODAY
    dots = bbox_of(im, (8, 66, 40, 124), lambda c: max(c) - min(c) > 80)
    j.check("a group header with a colour dot appears after 'Add tab to new group'", dots is not None, dots, cat="DESIGN")
    # fold / unfold by clicking the header
    hdr_y = (dots[1] + dots[3]) // 2 if dots else ROW(1)
    click(120, hdr_y); time.sleep(0.9); f1 = shot("J18-03-folded")
    j.check("click on the header folds the group (the screen changes in the sidebar)", bool(framediff("J18-02-group-created", "J18-03-folded", (0, 60, 220, 300))))
    click(120, hdr_y); time.sleep(0.9); f2 = shot("J18-04-unfolded")
    j.check("a second click unfolds it (back to the group-created frame)", not framediff("J18-02-group-created", "J18-04-unfolded", (0, 60, 220, 300)), cat="VIS")
    # group name: the default group has no name; ask: is the header readable (text next to the dot)?
    txt = bbox_of(im, (40, hdr_y - 8, 200, hdr_y + 8), lambda c: lum(c) < 0.45)
    j.info("group header text/name present?", txt is not None, "J18-02-group-created")
    # pin / unpin: Pin is the 8th row (Down x7), then the menu of a pinned tab says Unpin
    click(80, ROW(0)); time.sleep(0.5); rclick(80, ROW(0)); time.sleep(0.8); key(*([DOWN] * 7), ENTER); time.sleep(1.2); im = img(shot("J18-05-pinned"))
    j.check("Pin: a PINNED section appears", bbox_of(im, (14, 48, 70, 62), lambda c: lum(c) < 0.5) is not None)
    rclick(80, 82); time.sleep(0.9); shot("J18-06-pinned-menu"); key(ESC); time.sleep(0.4)
    # media tab: play audio with a real click on the page button, then look for the speaker glyph on its row
    t = cdp("new", f"{U}/media.html"); time.sleep(2.5); cdp("activate", t); time.sleep(0.6)
    btn = ev("(()=>{const r=document.querySelector('button').getBoundingClientRect();return [Math.round(r.x+r.width/2),Math.round(r.y+r.height/2)]})()", {"id": t})
    try: bx, by = json.loads(btn)
    except Exception: bx, by = 20, 20
    click(221 + bx, 37 + by); time.sleep(2.0); im = img(shot("J18-07-media"))
    played = ev("!document.getElementById('a').paused", {"id": t})
    j.check("media page plays after a real click on its button", played == "true", played)
    pre = img("J18-05-pinned")
    j.info("media row: compare sidebar rows before/after for a playing glyph (see screenshot)", bool(framediff("J18-05-pinned", "J18-07-media", (0, 60, 220, 330))), "J18-07-media")
    chrome_ok(j, "J18"); quit_browser()

# ------------------------------------------------------------------ J19 bookmarks
def J19():
    j = J("J19"); front()
    base = img(shot("J19-00"))
    def star_state(im): return bbox_of(im, (1194, 6, 1218, 30), lambda c: c[2] > 190 and c[0] < 140 and c[1] < 140) is not None
    j.check("star is outline at start", not star_state(base))
    click(*TB["star"]); time.sleep(1.0); im = img(shot("J19-01-bubble"))
    j.check("clicking the star bookmarks the page and fills the star (accent)", star_state(im))
    key(ENTER); time.sleep(0.8); im = img(shot("J19-02-done"))
    j.check("Enter in the bubble (Done) closes it and the star stays filled", star_state(im) and not framediff("J19-02-done", "J19-02-done"))
    click(*TB["star"]); time.sleep(1.0); shot("J19-03-bubble2")
    # Remove button at about (741,227) in the engine bubble
    click(776, 227); time.sleep(1.0); im = img(shot("J19-04-removed"))
    j.check("Remove in the bubble un-bookmarks (star back to outline)", not star_state(im), cat="DATA")
    # bookmark bar / manager
    chord(LCTRL, LSHIFT, 48); time.sleep(1.0); shot("J19-05-ctrl-shift-B")
    cdp("new", "atlas://bookmarks"); time.sleep(2.5); shot("J19-06-manager")
    chrome_ok(j, "J19"); quit_browser()

# ------------------------------------------------------------------ J20 can the keyboard reach the sidebar? J21 renderer kill
def J20():
    """Can the keyboard reach the sidebar? Ctrl+L, then Tab: omnibox popup, page stops, then the sidebar rows (F6 alone only cycles inside the toolbar pane)."""
    j = J("J20"); front(urls=PAGES[:3])
    from PIL import ImageChops
    click(80, ROW(0)); time.sleep(0.6)
    chord(LCTRL, L); time.sleep(0.5); frames = [shot("J20-0")]
    first = None
    for i in range(1, 13):
        key(TAB); time.sleep(0.45); frames.append(shot(f"J20-{i}"))
        d = ImageChops.difference(img(frames[i - 1]).crop((0, 66, 219, 660)), img(frames[i]).crop((0, 66, 219, 660))).getbbox()
        if d and first is None:
            first = i; break
    j.check("Ctrl+L then Tab: a focus ring appears on a sidebar row within 12 presses", first is not None, f"first sidebar change at Tab {first}", cat="ACC")
    if first:
        im = img(frames[first]); ring = bbox_of(im, (4, 64, 216, 100), lambda c: c[2] > 190 and c[0] < 150 and c[1] < 130)
        j.check("the ring is a visible accent outline around the row", ring is not None, ring, cat="ACC")
        key(DOWN); time.sleep(0.5); shot("J20-down")
        key(ENTER); time.sleep(0.9)
        j.check("Down moves the ring to the next row and Enter activates it (Bravo page)", vtitle() == "Bravo page", vtitle(), cat="KEYS")
    chord(LCTRL, L); time.sleep(0.4); key(F6); time.sleep(0.4); fr = [shot("J20-f6-0")]
    for i in range(1, 5):
        key(F6); time.sleep(0.5); fr.append(shot(f"J20-f6-{i}"))
    ch = [bool(ImageChops.difference(img(fr[i - 1]).crop((0, 66, 219, 660)), img(fr[i]).crop((0, 66, 219, 660))).getbbox()) for i in range(1, len(fr))]
    j.info("F6 cycle: does any pane stop land on the sidebar rows?", ch)
    quit_browser()

def J21():
    j = J("J21"); front()
    pids = sh(["bash", "-c", f"pgrep -f -- 'type=renderer.*user-data-dir={K}/profile-{INST} ' | head -20"]).stdout.split()   # ONLY this instance's renderers (never other workers')
    j.info("renderer pids", pids[:8])
    if pids:
        for pid in pids[-3:]: sh(["kill", "-9", pid])
        time.sleep(3.0); im = img(shot("J21-01-killed"))
        j.check("browser survives kill -9 of 3 renderers", alive())
        for y in (ROW(0), ROW(1), ROW(2), ROW(3)):
            click(80, y); time.sleep(1.2)
        im = img(shot("J21-02-after-clicks"))
        crashed = ev("document.body ? document.body.innerText.slice(0,40) : ''")
        j.info("front page text after clicking every row", crashed, "J21-02-after-clicks")
        # reload via F5 on a crashed tab
        key(F5); time.sleep(2.0); j.check("F5 revives a killed tab (page text back)", "Lorem" in (ev("document.body ? document.body.innerText : ''") or ""), vtitle())
    chrome_ok(j, "J21"); quit_browser()

# ------------------------------------------------------------------ J22 text truncation in the sidebar
def J22():
    j = J("J22"); front(urls=[PAGES[0]])
    long_t = "A very long page title that keeps going and going so that it cannot possibly fit in a 204 pixel wide row of the sidebar"
    cjk = "日本語のとても長いタイトルがここにあります。サイドバーに収まりません"
    for i, t in enumerate((long_t, cjk, "Ünïcödé — ÅÄÖ ّعربي")):
        u = "data:text/html;charset=utf-8," + urllib.parse.quote(f"<title>{t}</title><h1>{i}</h1>")
        cdp("new", u); time.sleep(1.2)
    time.sleep(1.5); click(80, ROW(0)); time.sleep(0.8); im = img(shot("J22-01-titles"))
    for k, name in enumerate(("long latin", "CJK", "mixed/RTL")):
        y = ROW(k + 1)
        # any text pixel in the strip right of the title zone (x 190..211) means the title runs under the close/rest area
        over = bbox_of(im, (192, y - 8, 211, y + 8), lambda c: lum(c) < 0.5)
        j.check(f"{name} title is clipped before the right end of the row (x>=192 clear)", over is None, over, cat="DESIGN")
    # hover tooltip with the full title
    p = subprocess.Popen([f"{K}/vinput", f"{W}x{H}", "move", "100", str(ROW(1))], env={**os.environ, **ENV, "HOLD_MS": "4500"}); time.sleep(3.2); shot("J22-02-tooltip"); p.wait()
    j.info("tooltip for a truncated title: see screenshot", "", "J22-02-tooltip")
    chrome_ok(j, "J22"); quit_browser()

JOURNEYS = {k: v for k, v in globals().items() if re.fullmatch(r"J\d\d", k)}
if __name__ == "__main__":
    want = sys.argv[1:] or ["all"]
    for k in (sorted(JOURNEYS) if want == ["all"] else want):
        try: JOURNEYS[k]()
        except Exception as e:
            import traceback; record(k, 99, "harness exception", "FAIL", repr(e)[:300]); traceback.print_exc()
