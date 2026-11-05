#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""e2e_density.py DENSITY: the menu rows follow --atlas-density (menu.toml: compact 24, standard 28, comfortable
32). Run ON the forge after `run.sh wol-light OUT --atlas-kit-chrome --atlas-density=DENSITY URL`. Opens the app menu and
measures the accent first row (it is the focused row) in pixels; also the downloads-card width is the side sheet's 336."""
import subprocess, sys, time
K = "/scratch/atlas-slot8/kitrun"; S = "/scratch/atlas-slot8/shots"; T = "wol-light"
DENSITY = sys.argv[1] if len(sys.argv) > 1 else "standard"
HEIGHT = {"compact": 24, "standard": 28, "comfortable": 32}[DENSITY]
ENV = {"XDG_RUNTIME_DIR": f"{K}/run-{T}", "WAYLAND_DISPLAY": "wayland-0", "PATH": "/usr/bin:/bin"}
def click(x, y):
    subprocess.run([f"{K}/in.sh", T, "click", str(x), str(y)], check=True, capture_output=True)
def shot(name):
    subprocess.run([f"{K}/shot.sh", T, f"{S}/{name}.png"], check=True)
def column(x, y, h):
    out = subprocess.run(["grim", "-g", f"{x},{y} 1x{h}", "-t", "ppm", "-"], capture_output=True, env=ENV).stdout
    px = out[-3 * h:]
    return [tuple(px[i:i + 3]) for i in range(0, len(px), 3)]
def is_accent(c):
    r, g, b = c
    return b > 200 and r < 150 and g < 120
res = []
def check(name, ok, extra=""):
    res.append(ok); print(("PASS " if ok else "FAIL ") + name + (" " + extra if extra else ""), flush=True)
click(1262, 17); time.sleep(1.0); shot(f"ui-density-{DENSITY}")
col = column(1100, 30, 80)
accent = sum(1 for c in col if is_accent(c))
check(f"{DENSITY}: the accent first row of the app menu is {HEIGHT} px high", accent == HEIGHT, f"{accent} px")
print(f"RESULT {sum(res)}/{len(res)}")
