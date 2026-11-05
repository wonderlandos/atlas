#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""e2e_a9.py: the Kit chrome journeys of AUDIT-9 WEB-ATLAS (rows 6, 7, 11, 13, 15) and ATLAS-E2E (03, 04, 09, 10, 11,
12, 13, 14, 15). Run ON the forge as `python3 e2e_a9.py [wol-light]`; it starts the slot 8 browser itself (run.sh) in each
scenario. It needs a page server on 127.0.0.1:8147 serving p1.html..p40.html (<title>Page N</title>), audio.html (a
playing <audio>, title Music) and alpha/bravo/charlie/delta.html: `python3 -m http.server 8147 --bind 127.0.0.1` in
/scratch/atlas-slot8/a9www after atlas/tests/kit/a9_pages.sh. One line per check (PASS/FAIL)."""
import json, subprocess, sys, time
from PIL import Image
K = "/scratch/atlas-slot8/kitrun"; S = "/scratch/atlas-slot8/shots"; T = sys.argv[1] if len(sys.argv) > 1 else "wol-light"
PORT = "9348"; B = "http://127.0.0.1:8147/"
ENV = {"XDG_RUNTIME_DIR": f"{K}/run-{T}", "WAYLAND_DISPLAY": "wayland-0", "PATH": "/usr/bin:/bin"}
def cdp(*a):
    return subprocess.run(["python3", "-I", f"{K}/cdp.py", PORT, *a], capture_output=True, text=True, timeout=60).stdout.strip()
def tabs():
    return json.loads(cdp("tabs"))
def inp(*a):
    subprocess.run([f"{K}/in.sh", T, *map(str, a)], check=True, capture_output=True)
def key(*k): inp("key", 194, *k)   # the virtual keyboard drops the first key of every call
def shot(name):
    subprocess.run([f"{K}/shot.sh", T, f"{S}/{name}.png"], check=True); return Image.open(f"{S}/{name}.png").convert("RGB")
def start(urls, *flags):
    subprocess.run([f"{K}/run.sh", T, "/scratch/atlas-slot8/a9out", "--atlas-kit-chrome", *flags, *urls], env={"WAIT": "9", "PATH": "/usr/bin:/bin"}, capture_output=True, timeout=120)
def accent(px): return px[2] > 200 and px[0] < 150 and px[1] < 120 and px[0] > 90
def count(im, box, pred):
    x0, y0, x1, y1 = box; return sum(1 for x in range(x0, x1) for y in range(y0, y1) if pred(im.getpixel((x, y))))
res = []
def check(name, ok, extra=""):
    res.append(ok); print(("PASS " if ok else "FAIL ") + name + (" " + extra if extra else ""), flush=True)

# ---- 30 tabs: ATLAS-E2E-03 / WEB-ATLAS-7 (scroll), WEB-ATLAS-6 (rows follow a tab in place)
start([f"{B}p{i}.html" for i in range(1, 31)])
check("30 tabs open", len(tabs()) == 30, str(len(tabs())))
key("+29", 10, "-29"); time.sleep(1.5)   # Ctrl+9: the last tab
im = shot("a9-30tabs-last")
check("Ctrl+9 scrolls the list: the pill of tab 30 is inside the visible list (above the footer)", count(im, (8, 600, 212, 662), lambda c: abs(c[0] - 213) < 8 and abs(c[2] - 249) < 8) > 500)
check("... and Page 30 is the row text there", count(im, (40, 636, 120, 654), lambda c: sum(c) < 200) > 20)
# focus survives other tabs' title changes (rows are updated in place, not rebuilt)
inp("click", 640, 17); time.sleep(0.4)
for _ in range(3):
    key(15); time.sleep(0.3)
key(108); time.sleep(0.4)
ring = lambda c: abs(c[0] - 116) < 25 and abs(c[2] - 245) < 20 and c[1] < 120
before = count(shot("a9-focus-1"), (0, 40, 220, 670), ring)
ids = [t["id"] for t in tabs()[:6]]
for n in range(6):
    for i in ids:
        cdp("eval", i, f"document.title='Retitled {n} '+Math.random()")
    time.sleep(0.3)
after = count(shot("a9-focus-2"), (0, 40, 220, 670), ring)
check("a focused row keeps its focus ring while other tabs change their title 36 times", before > 200 and after > 200, f"ring px {before} -> {after}")

# ---- audio glyph and mute (E2E-04), group header (E2E-13), crashed tab title (E2E-14), F6 (E2E-15)
start([f"{B}audio.html", f"{B}p1.html", f"{B}p2.html"], "--autoplay-policy=no-user-gesture-required", "--disable-audio-output")
time.sleep(3); inp("click", 80, 138); time.sleep(2)
im = shot("a9-audio-1")
check("a tab with sound shows the speaker glyph in the accent colour", count(im, (162, 72, 182, 92), accent) > 8)
inp("click", 172, 82); time.sleep(1.5)
im = shot("a9-audio-2")
check("a click on the glyph mutes the tab (muted glyph, dim) without leaving the page", count(im, (162, 72, 182, 92), accent) == 0 and count(im, (162, 72, 182, 92), lambda c: sum(c) < 400) > 8 and "p2.html" in json.dumps([t["url"] for t in tabs() if cdp("eval", t["id"], "document.visibilityState") == '"visible"']))
inp("click", 80, 110, 273); time.sleep(1); inp("click", 150, 187); time.sleep(1.2); key(1); time.sleep(1)
im = shot("a9-group")
check("a new group has a label ('UNTITLED GROUP') and a count, its member is indented", count(im, (16, 100, 125, 116), lambda c: sum(c) < 450) > 40 and count(im, (170, 100, 184, 116), lambda c: sum(c) < 450) > 1 and count(im, (24, 125, 36, 143), lambda c: sum(c) < 450) > 30)
cdp("new", "chrome://crash"); time.sleep(3)
im = shot("a9-crash")
check("a crashed tab's row is titled by its host, not empty", count(im, (40, 200, 120, 228), lambda c: sum(c) < 200) > 15)

# ---- F6 reaches the sidebar (E2E-15), the lines are 1 px (E2E-11), the omnibox is 24 px (E2E-10), the default accent (E2E-09)
start([f"{B}p1.html", f"{B}p2.html", f"{B}p3.html"])
im0 = shot("a9-resting")   # nothing focused yet: the omnibox pill is measured without its focus ring
bg0 = im0.getpixel((640, 1))
pill0 = [y for y in range(0, 36) if im0.getpixel((640, y)) != bg0]
inp("click", 640, 17); time.sleep(0.5)
seen = []
for _ in range(4):
    key(64); time.sleep(0.6)
    seen.append(count(shot("a9-f6"), (0, 40, 220, 200), ring))
check("F6 x4 visits the sidebar (its pill and ring are accent) once", sum(1 for n in seen if n > 300) == 1, str(seen))
im = shot("a9-lines")
cols = [x for x in range(210, 232) if abs(im.getpixel((x, 300))[0] - 215) < 4 and abs(im.getpixel((x, 300))[2] - 224) < 4]
rows_ = [y for y in range(30, 44) if abs(im.getpixel((600, y))[0] - 215) < 4 and abs(im.getpixel((600, y))[2] - 224) < 4]
rows_l = [y for y in range(30, 44) if abs(im.getpixel((100, y))[0] - 215) < 4 and abs(im.getpixel((100, y))[2] - 224) < 4]
check("the sidebar edge is one column wide", len(cols) == 1, str(cols))
check("the line under the toolbar is one row high over the page and over the sidebar, in the same row", rows_ == rows_l and len(rows_) == 1, f"{rows_} {rows_l}")
check("the omnibox is 24 px high, border included", bool(pill0) and pill0[-1] - pill0[0] + 1 == 24, str((pill0[0], pill0[-1]) if pill0 else None))
# ---- E2E-09: an empty profile (no system_theme preference seeded) wears the WolOS look all the same
src = open(f"{K}/run.sh").read().replace("cat > $P/Default/Preferences <<S", "cat > /dev/null <<S")
open(f"{K}/run-noseed.sh", "w").write(src); subprocess.run(["chmod", "+x", f"{K}/run-noseed.sh"])
subprocess.run([f"{K}/run-noseed.sh", T, "/scratch/atlas-slot8/a9out", "--atlas-kit-chrome", f"{B}p1.html"], env={"WAIT": "9", "PATH": "/usr/bin:/bin"}, capture_output=True, timeout=120)
im = shot("a9-noseed")
check("a profile without the system_theme preference has the WolOS glass toolbar and accent pill", im.getpixel((300, 17)) == (241, 241, 250) and abs(im.getpixel((150, 82))[0] - 213) < 6 and abs(im.getpixel((150, 82))[2] - 249) < 6, str((im.getpixel((300, 17)), im.getpixel((150, 82)))))
print(f"RESULT {sum(res)}/{len(res)}")
