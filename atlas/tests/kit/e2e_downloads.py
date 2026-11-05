#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""e2e_downloads.py: the Kit downloads list (reel scene 7, patch 0103). Run ON the forge after
`run.sh wol-light OUT --atlas-kit-chrome URL` with the lane's server on 127.0.0.1:8099 (/slowdl streams 3 MB in
15 s as big-trip.zip, /porto-walks.pdf is a finished one). One line per check; screenshots in the shots dir."""
import subprocess, sys, time
K = "/scratch/atlas-slot8/kitrun"; S = "/scratch/atlas-slot8/shots"; T = sys.argv[1] if len(sys.argv) > 1 else "wol-light"
PORT = "9348"
ENV = {"XDG_RUNTIME_DIR": f"{K}/run-{T}", "WAYLAND_DISPLAY": "wayland-0", "PATH": "/usr/bin:/bin"}
def cdp(*a):
    return subprocess.run(["python3", "-I", f"{K}/cdp.py", PORT, *a], capture_output=True, text=True, timeout=60).stdout.strip()
def click(x, y, button=272):
    subprocess.run([f"{K}/in.sh", T, "click", str(x), str(y), str(button)], check=True, capture_output=True)
def move(x, y):
    subprocess.run([f"{K}/in.sh", T, "move", str(x), str(y)], check=True, capture_output=True)
def shot(name):
    subprocess.run([f"{K}/shot.sh", T, f"{S}/{name}.png"], check=True)
def pixel(x, y):
    out = subprocess.run(["grim", "-g", f"{x},{y} 1x1", "-t", "ppm", "-"], capture_output=True, env=ENV).stdout
    return tuple(out[-3:])
def any_accent(x, y, w=16, h=16):   # a thin glyph: look at the box around it
    out = subprocess.run(["grim", "-g", f"{x},{y} {w}x{h}", "-t", "ppm", "-"], capture_output=True, env=ENV).stdout
    px = out[-3 * w * h:]
    return any(px[i + 2] > 200 and px[i] < 150 and px[i + 1] < 120 for i in range(0, len(px), 3))
def accent(x, y):
    r, g, b = pixel(x, y)
    return b > 200 and r < 150 and g < 120
res = []
def check(name, ok, extra=""):
    res.append(ok); print(("PASS " if ok else "FAIL ") + name + (" " + extra if extra else ""), flush=True)

cdp("new", "http://127.0.0.1:8099/porto-walks.pdf"); time.sleep(2)
cdp("new", "http://127.0.0.1:8099/slowdl"); time.sleep(3)
click(1234, 17); time.sleep(1.5); shot("ui-dl-1-list")
check("list opens as the Kit card: accent document glyph on the rows", any_accent(923, 82) and any_accent(923, 134))
bar1 = accent(1090, 110); time.sleep(5); shot("ui-dl-2-later")
check("a running download shows a progress line that grows (refreshed live)", not bar1 and accent(1090, 110))
# (hover paint of popups cannot be checked under cage: pointer motion over a popup surface sets no hover state there)
click(1000, 142, 273); time.sleep(1.2); shot("ui-dl-3-rowmenu")
check("right click on a row opens the row menu as a Kit popover (accent row)", accent(950, 163) or accent(950, 170) or accent(950, 175))
print(f"RESULT {sum(res)}/{len(res)}")
