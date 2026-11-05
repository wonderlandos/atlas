#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""e2e_rest.py: resting tabs in the Kit sidebar (reel scene R09). Run ON the forge after
`run.sh wol-light OUT --atlas-kit-chrome URL-A URL-B URL-C URL-D` (titles Alpha, Bravo, Charlie, Delta; run.sh turns
the internal pages on): Bravo and Charlie are discarded through the discards page; their rows must show the frost mark
in the accent colour at the right end and a dim title; a click wakes one (its mark goes, the other keeps it)."""
import json, subprocess, sys, time
K = "/scratch/atlas-slot8/kitrun"; S = "/scratch/atlas-slot8/shots"; T = sys.argv[1] if len(sys.argv) > 1 else "wol-light"
PORT = "9348"
ENV = {"XDG_RUNTIME_DIR": f"{K}/run-{T}", "WAYLAND_DISPLAY": "wayland-0", "PATH": "/usr/bin:/bin"}
JS = r"""(async()=>{const rowsNow=()=>{const out=[];const visit=n=>{if(!n)return;if(n.shadowRoot)visit(n.shadowRoot);
 for(const c of (n.children||[])){out.push(c);visit(c)}};visit(document.body);return out.filter(e=>e.tagName==='TR')};
 let n=0;for(const t of ['Bravo','Charlie']){for(let k=0;k<20;k++){const r=rowsNow().find(r=>(r.innerText||'').includes(t));
  const a=r&&[...r.querySelectorAll('div')].find(e=>/^\[Urgent [Dd]iscard\]$/.test((e.textContent||'').trim()));
  if(a){a.click();n++;break}await new Promise(z=>setTimeout(z,200));}
  await new Promise(z=>setTimeout(z,1200));}
 return n})()"""
def cdp(*a):
    return subprocess.run(["python3", "-I", f"{K}/cdp.py", PORT, *a], capture_output=True, text=True, timeout=60).stdout.strip()
def click(x, y):
    subprocess.run([f"{K}/in.sh", T, "click", str(x), str(y)], check=True, capture_output=True)
def shot(name):
    subprocess.run([f"{K}/shot.sh", T, f"{S}/{name}.png"], check=True)
def region(x, y, w, h):
    out = subprocess.run(["grim", "-g", f"{x},{y} {w}x{h}", "-t", "ppm", "-"], capture_output=True, env=ENV).stdout
    px = out[-3 * w * h:]
    return [tuple(px[i:i + 3]) for i in range(0, len(px), 3)]
def has_accent(y):   # the frost mark: accent-coloured pixels in the right end of the row at y
    return any(b > 200 and r < 200 and abs(r - 0x74) + abs(g - 0x52) + abs(b - 0xf5) < 120 for r, g, b in region(184, y - 12, 24, 24))
res = []
def check(name, ok, extra=""):
    res.append(ok); print(("PASS " if ok else "FAIL ") + name + (" " + extra if extra else ""), flush=True)
ROW = lambda i: 82 + 28 * i
d = cdp("new", "chrome://discards"); time.sleep(4)
n = cdp("eval", d, JS); cdp("close", d); time.sleep(1.5)
check("two background tabs put to rest", n == "2", str(n))
click(80, ROW(0)); time.sleep(1.0); shot("ui-rest-1")
check("Bravo and Charlie rows carry the frost mark", has_accent(ROW(1)) and has_accent(ROW(2)))
check("rows awake (Alpha in front, Delta) carry none", not has_accent(ROW(0)) and not has_accent(ROW(3)))
click(80, ROW(1)); time.sleep(2.0); shot("ui-rest-2-woken")
check("a click wakes Bravo: its mark is gone, Charlie's stays", not has_accent(ROW(1)) and has_accent(ROW(2)))
print(f"RESULT {sum(res)}/{len(res)}")
