#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""e2e.py: drive the Kit chrome on slot 8 with real pointer clicks (vinput) and check the engine state over CDP.
Prints one line per check (PASS/FAIL), latencies in ms, and writes screenshots to the shots dir."""
import json, subprocess, sys, time
K = "/scratch/atlas-slot8/kitrun"; S = "/scratch/atlas-slot8/shots"; T = sys.argv[1] if len(sys.argv) > 1 else "wol-light"
PORT = "9348"
def cdp(*a):
    return subprocess.run(["python3", "-I", f"{K}/cdp.py", PORT, *a], capture_output=True, text=True, timeout=30).stdout.strip()
def tabs():
    return json.loads(cdp("tabs"))
def click(x, y):
    subprocess.run([f"{K}/in.sh", T, "click", str(x), str(y)], check=True, capture_output=True)
def shot(name):
    subprocess.run([f"{K}/shot.sh", T, f"{S}/{name}.png"], check=True)
def visible():
    for t in tabs():
        if cdp("eval", t["id"], "document.visibilityState") == "\"visible\"":
            return t
    return None
def wait(cond, timeout=5.0):
    t0 = time.monotonic()
    while time.monotonic() - t0 < timeout:
        v = cond()
        if v:
            return (time.monotonic() - t0) * 1000, v
        time.sleep(0.01)
    return None, None
res = []
def check(name, ok, extra=""):
    res.append(ok); print(("PASS " if ok else "FAIL ") + name + (" " + extra if extra else ""), flush=True)

first = tabs()[-1] if tabs() else None
v = visible(); check("one tab visible at start", v is not None, v["title"] if v else "")
# navigate the front tab to a second page so Back has somewhere to go
B = "data:text/html,<title>Notes, part two</title><h1>Part two</h1>"
cdp("nav", v["id"], B); time.sleep(1)
shot("kit-e2e-0-after-nav")
click(46, 17)  # Back
ms, ok = wait(lambda: "Release" in (visible() or {}).get("title", ""))
check("Back button goes back", ok is not None, f"{ms:.0f} ms" if ms else "")
click(74, 17)  # Forward
ms, ok = wait(lambda: "part two" in (visible() or {}).get("title", ""))
check("Forward button goes forward", ok is not None, f"{ms:.0f} ms" if ms else "")
cdp("eval", visible()["id"], "window.__mark = 1")
click(102, 17)  # Reload
ms, ok = wait(lambda: cdp("eval", visible()["id"], "window.__mark === undefined") == "true")
check("Reload button reloads", ok is not None, f"{ms:.0f} ms" if ms else "")
n = len(tabs())
click(65, 690)  # footer New tab
ms, ok = wait(lambda: len(tabs()) == n + 1)
check("Footer New tab opens a tab", ok is not None, f"{ms:.0f} ms, tabs {n} -> {len(tabs())}" if ms else "")
time.sleep(1); shot("kit-e2e-1-new-tab")
# activate the 2nd row (Train times) in the sidebar; rows are 28 px from y=82 (one TODAY header above)
click(80, 110)
ms, ok = wait(lambda: "Train" in (visible() or {}).get("title", ""))
check("Sidebar row click selects that tab", ok is not None, f"{ms:.0f} ms" if ms else "")
time.sleep(0.5); shot("kit-e2e-2-row-selected")
lat = []
for y in (82, 110, 82, 110):
    t0 = time.monotonic(); click(80, y)
    ms, ok = wait(lambda: (visible() or {}).get("title", "") != "", 3)
    lat.append(ms or -1)
print("INFO tab switch click->visible (ms, includes CDP polling):", [round(x) for x in lat])
click(18, 17)  # sidebar toggle
time.sleep(1.2); shot("kit-e2e-3-collapsed")
click(18, 17)
time.sleep(1.2); shot("kit-e2e-4-expanded")
print(f"RESULT {sum(res)}/{len(res)}")
