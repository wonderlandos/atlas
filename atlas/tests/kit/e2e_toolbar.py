#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""e2e_toolbar.py: the Kit toolbar beyond e2e.py: reload becomes stop while a page loads and stops it; a right click
on back lists the earlier pages as a Kit popover and a row goes there. Run ON the forge after
`run.sh wol-light OUT --atlas-kit-chrome URL` with the lane's server (127.0.0.1:8099, /slow answers after 12 s)."""
import json, subprocess, sys, time
K = "/scratch/atlas-slot8/kitrun"; S = "/scratch/atlas-slot8/shots"; T = sys.argv[1] if len(sys.argv) > 1 else "wol-light"
PORT = "9348"
ENV = {"XDG_RUNTIME_DIR": f"{K}/run-{T}", "WAYLAND_DISPLAY": "wayland-0", "PATH": "/usr/bin:/bin"}
def cdp(*a):
    return subprocess.run(["python3", "-I", f"{K}/cdp.py", PORT, *a], capture_output=True, text=True, timeout=60).stdout.strip()
def tabs():
    return json.loads(cdp("tabs"))
def click(x, y, button=272):
    subprocess.run([f"{K}/in.sh", T, "click", str(x), str(y), str(button)], check=True, capture_output=True)
def shot(name):
    subprocess.run([f"{K}/shot.sh", T, f"{S}/{name}.png"], check=True)
def pixel(x, y):
    out = subprocess.run(["grim", "-g", f"{x},{y} 1x1", "-t", "ppm", "-"], capture_output=True, env=ENV).stdout
    return tuple(out[-3:])
def accent(x, y):
    r, g, b = pixel(x, y)
    return b > 200 and r < 150 and g < 120
res = []
def check(name, ok, extra=""):
    res.append(ok); print(("PASS " if ok else "FAIL ") + name + (" " + extra if extra else ""), flush=True)

tab = tabs()[0]["id"]
cdp("nav", tab, "data:text/html,<title>Bravo</title><h1>Bravo</h1>"); time.sleep(1)
# a slow page: the load bar is under the omnibox (left end, accent) while it loads
subprocess.Popen(["python3", "-I", f"{K}/cdp.py", PORT, "nav", tab, "http://127.0.0.1:8099/slow?e2e"], stdout=subprocess.DEVNULL)
time.sleep(2.0); shot("ui-toolbar-1-loading")
check("page loading: load bar shows under the omnibox", accent(375, 29), str(pixel(375, 29)))
click(102, 17); time.sleep(1.0); shot("ui-toolbar-2-stopped")
check("the reload button stops the load (load bar gone)", not accent(375, 29), str(pixel(375, 29)))
time.sleep(0.5)
# back history menu
click(46, 17, 273); time.sleep(1.0); shot("ui-toolbar-3-backmenu")
check("right click on back opens the history as a Kit popover (accent row)", accent(150, 38))
click(150, 38); time.sleep(1.5)   # the only earlier page: Alpha (the slow page never committed)
t = [x for x in tabs()][0]
check("a row of it goes to that page", t["title"] == "Alpha", t["title"])
print(f"RESULT {sum(res)}/{len(res)}")
