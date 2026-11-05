#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""e2e_lib.py: helpers for e2e_full.py (ATLAS-E2E). Runs ON the forge. Real pointer/keyboard events through vinput
(wlroots virtual devices under cage), page state through CDP (cdp.py), pixels through grim + PIL. Own instance
(e2e_full.sh NAME): INST env var picks it (default 'a')."""
import json, os, subprocess, sys, time
from PIL import Image
K = "/scratch/atlas-e2e"; INST = os.environ.get("INST", "a"); PORT = os.environ.get("PORT", "9360")
THEME = os.environ.get("THEME", "wol-light"); W, H = 1280, 720
RUN = f"{K}/run-{INST}"; SHOTS = f"{K}/shots"; RESULTS = f"{K}/results.jsonl"
ENV = {"XDG_RUNTIME_DIR": RUN, "WAYLAND_DISPLAY": "wayland-0", "PATH": "/usr/bin:/bin"}
def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, env={**os.environ, **ENV}, **kw)
def cdp(*a, timeout=30):
    try:
        return subprocess.run(["python3", "-I", f"{K}/cdp.py", PORT, *a], capture_output=True, text=True, timeout=timeout).stdout.strip()
    except subprocess.TimeoutExpired:
        return "TIMEOUT"
def tabs():
    try: return json.loads(cdp("tabs"))
    except Exception: return []
def vin(*a):
    return subprocess.run([f"{K}/vinput", f"{W}x{H}", *map(str, a)], capture_output=True, env={**os.environ, **ENV}, timeout=60)
def click(x, y, b=272): vin("click", x, y, b)
def rclick(x, y): vin("click", x, y, 273)
def mclick(x, y): vin("click", x, y, 274)
def move(x, y): vin("move", x, y)
def drag(x, y, x2, y2): vin("drag", x, y, x2, y2)
def key(*k): vin("key", 194, *k)   # vinput drops the first key event of every invocation (harness bug); 194 = F24 is a harmless sacrificial key
# evdev codes
ESC, ENTER, TAB, SPACE, BKSP, LSHIFT, LCTRL, LALT, LMETA = 1, 28, 15, 57, 14, 42, 29, 56, 125
UP, DOWN, LEFT, RIGHT, HOME, END, PGDN, PGUP, F5, F6, F11, F12 = 103, 108, 105, 106, 102, 107, 109, 104, 63, 64, 87, 88
L, T, W_, F, D, N, R, S, P, K_, A_ = 38, 20, 17, 33, 32, 49, 19, 31, 25, 37, 30
def chord(*codes):
    """hold all but the last, tap the last, release (modifiers first)"""
    key(*[f"+{c}" for c in codes[:-1]], codes[-1], *[f"-{c}" for c in reversed(codes[:-1])])
def type_text(s):
    CODES = {**{c: n for n, c in enumerate("qwertyuiop", 16)}, **{c: n for n, c in enumerate("asdfghjkl", 30)},
             **{c: n for n, c in enumerate("zxcvbnm", 44)}, **{c: n for n, c in enumerate("1234567890", 2)},
             " ": 57, ".": 52, "/": 53, "-": 12, ":": None}
    seq = []
    for ch in s:
        if ch == ":": seq += [f"+{LSHIFT}", 39, f"-{LSHIFT}"]
        elif ch.lower() in CODES and CODES[ch.lower()]: seq += [CODES[ch.lower()]]
    key(*seq)
def shot(name):
    p = f"{SHOTS}/{name}.png"; sh(["grim", p]); return p
def img(name):
    p = name if name.startswith("/") else f"{SHOTS}/{name}.png"
    return Image.open(p).convert("RGB")
def px(im, x, y): return im.getpixel((x, y))
def visible():
    for t in tabs():
        if cdp("eval", t["id"], "document.visibilityState") == "\"visible\"": return t
    return None
def vtitle(): return (visible() or {}).get("title", "")
def vurl(): return (visible() or {}).get("url", "")
def wait(cond, timeout=6.0, step=0.05):
    t0 = time.monotonic()
    while time.monotonic() - t0 < timeout:
        try: v = cond()
        except Exception: v = None
        if v: return (time.monotonic() - t0) * 1000, v
        time.sleep(step)
    return None, None
def ev(expr, tab=None):
    t = tab or visible()
    return cdp("eval", t["id"], expr) if t else ""
def ensure_server():
    import urllib.request
    try: urllib.request.urlopen("http://127.0.0.1:8123/alpha.html", timeout=3); return True
    except Exception: return False
def alive():
    return sh(["pgrep", "-f", f"user-data-dir={K}/profile-{INST} "]).stdout.split() != [] and bool(tabs())
# ---------- results
def record(jid, step, name, status, note="", ev_path="", cat=""):
    row = {"j": jid, "step": step, "name": name, "status": status, "note": note, "evidence": ev_path, "cat": cat, "t": time.strftime("%H:%M:%S")}
    with open(RESULTS, "a") as f: f.write(json.dumps(row) + "\n")
    print(f"{status:5} {jid}.{step:02d} {name}" + (f"  [{note}]" if note else ""), flush=True)
class J:
    def __init__(self, jid): self.jid = jid; self.n = 0
    def check(self, name, ok, note="", shotname="", cat=""):
        self.n += 1; record(self.jid, self.n, name, "PASS" if ok else "FAIL", str(note), shotname, cat); return ok
    def info(self, name, note="", shotname=""):
        self.n += 1; record(self.jid, self.n, name, "INFO", str(note), shotname)
    def skip(self, name, why):
        self.n += 1; record(self.jid, self.n, name, "SKIP", why)
# ---------- pixel measures (Kit-lint on a screenshot)
def lum(c): return (0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]) / 255
def _lin(v):
    v /= 255; return v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4
def rl(c): return 0.2126 * _lin(c[0]) + 0.7152 * _lin(c[1]) + 0.0722 * _lin(c[2])
def contrast(a, b):
    x, y = rl(a), rl(b); x, y = max(x, y), min(x, y); return (x + 0.05) / (y + 0.05)
def bbox_of(im, box, pred):
    """bbox (x0,y0,x1,y1) of pixels in box=(x0,y0,x1,y1) satisfying pred(rgb); None if none"""
    x0, y0, x1, y1 = box; xs, ys = [], []
    for y in range(y0, y1):
        for x in range(x0, x1):
            if pred(im.getpixel((x, y))): xs.append(x); ys.append(y)
    return (min(xs), min(ys), max(xs), max(ys)) if xs else None
def runs(im, y, x0, x1, pred):
    """horizontal runs [(a,b)] of pixels satisfying pred along row y"""
    out, a = [], None
    for x in range(x0, x1):
        ok = pred(im.getpixel((x, y)))
        if ok and a is None: a = x
        if not ok and a is not None: out.append((a, x - 1)); a = None
    if a is not None: out.append((a, x1 - 1))
    return out
def vruns(im, x, y0, y1, pred):
    out, a = [], None
    for y in range(y0, y1):
        ok = pred(im.getpixel((x, y)))
        if ok and a is None: a = y
        if not ok and a is not None: out.append((a, y - 1)); a = None
    if a is not None: out.append((a, y1 - 1))
    return out
def dist(a, b): return max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2]))
def quit_browser():
    p = f"{RUN}/cage.pid"
    if os.path.exists(p):
        pid = open(p).read().strip()
        sh(["pkill", "-TERM", "-P", pid]); sh(["kill", "-TERM", pid]); time.sleep(1.5)
def start(name_flags, theme=None, inst=None, wait_s=8, env=None):
    e = {**os.environ, "PORT": PORT, **(env or {})}
    subprocess.run([f"{K}/e2e_full.sh", theme or THEME, inst or INST, *name_flags], env=e, capture_output=True, timeout=120)
    t0 = time.monotonic()
    while time.monotonic() - t0 < 30 and not tabs(): time.sleep(0.5)
    time.sleep(1.5)
# ---------- deep text of a WebUI page (walks shadow roots)
DEEP_JS = r"""(()=>{const out=[];const walk=n=>{if(!n)return;if(n.nodeType===3){const t=n.textContent.trim();if(t)out.push(t);return}
 if(n.nodeType===1){const s=getComputedStyle(n);if(s.display==='none'||s.visibility==='hidden')return;
  for(const a of ['aria-label','title','placeholder','label']){const v=n.getAttribute&&n.getAttribute(a);if(v)out.push(v)}
  if(n.shadowRoot)for(const c of n.shadowRoot.childNodes)walk(c)}
 for(const c of (n.childNodes||[]))walk(c)};walk(document.body);return out.join('\n')})()"""
def deep_text(url, wait_s=3.5, close=True):
    t = cdp("new", url); time.sleep(wait_s)
    txt = cdp("eval", t, DEEP_JS)
    try: txt = json.loads(txt)
    except Exception: pass
    if close: cdp("close", t)
    return txt if isinstance(txt, str) else ""
