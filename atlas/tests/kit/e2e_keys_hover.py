#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""e2e_keys_hover.py: keyboard and hover in the Kit popups and rows. Run ON the forge after
`run.sh wol-light OUT --atlas-kit-chrome URL-A URL-B` (titles Alpha, Bravo) with the lane's download server on :8099.
Keys: every call starts with a throw-away key (evdev 194: the virtual keyboard drops the first); hover: hover.sh keeps
the pointer alive while the screenshot is taken. One line per check (PASS/FAIL)."""
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
def key(*codes):
    subprocess.run([f"{K}/in.sh", T, "key", "194", *map(str, codes)], check=True, capture_output=True)
def hover(x, y, ms=2500):
    subprocess.run([f"{K}/hover.sh", T, str(x), str(y), str(ms)], check=True, capture_output=True)
def shot(name):
    subprocess.run([f"{K}/shot.sh", T, f"{S}/{name}.png"], check=True)
def pixel(x, y):
    out = subprocess.run(["grim", "-g", f"{x},{y} 1x1", "-t", "ppm", "-"], capture_output=True, env=ENV).stdout
    return tuple(out[-3:])
def accent(x, y):
    r, g, b = pixel(x, y)
    return b > 200 and r < 150 and g < 120
def wait(cond, timeout=5.0):
    t0 = time.monotonic()
    while time.monotonic() - t0 < timeout:
        v = cond()
        if v:
            return (time.monotonic() - t0) * 1000, v
        time.sleep(0.05)
    return None, None
res = []
def check(name, ok, extra=""):
    res.append(ok); print(("PASS " if ok else "FAIL ") + name + (" " + extra if extra else ""), flush=True)
def settle():
    time.sleep(3.0)   # a hover.sh pointer lives 2.5 s

MENU = (1262, 17); PRIVACY = (1178, 17); DOWNLOADS = (1234, 17)
# ---- hover: app menu (the accent follows the pointer, only one row at a time)
click(*MENU); time.sleep(0.8)
hover(1100, 385); shot("ui-hover-1-menu")
check("app menu: the row under the pointer is the accent row", accent(1100, 387))
check("... and the first row is not (one accent row at a time)", not accent(1100, 52))
settle(); key(1); time.sleep(0.5)
# ---- privacy popover: keyboard (Enter on the only row opens Settings), Esc closes
click(*PRIVACY); time.sleep(1.0)
check("privacy popover: its link row is the accent row", accent(1000, 437) or accent(1000, 420) or accent(1000, 450))
key(1); time.sleep(0.6)
check("privacy popover: Esc closes it", not (accent(1000, 437) or accent(1000, 420) or accent(1000, 450)))
click(*PRIVACY); time.sleep(1.0); n = len(tabs()); key(28)
ms, ok = wait(lambda: len(tabs()) == n + 1 and any("settings/privacy" in t["url"] for t in tabs()))
check("privacy popover: Enter on 'Privacy settings' opens the settings page", ok is not None)
time.sleep(0.8)
# ---- downloads: list opens, row menu with the keyboard, hover on a row
cdp("new", "http://127.0.0.1:8099/porto-walks.pdf"); time.sleep(3)
click(*DOWNLOADS); time.sleep(1.5)
hover(1080, 100); shot("ui-hover-2-downloads")
check("downloads: the hovered row is the accent pill", pixel(1080, 72) != pixel(1080, 40), str(pixel(1080, 72)))
settle()
click(1080, 100, 273); time.sleep(1.2)
check("downloads row menu: first row is the accent row", accent(950, 121))
key(108); time.sleep(0.4); shot("ui-keys-1-rowmenu")
check("downloads row menu: Down moves the accent to the second row", accent(950, 149) and not accent(950, 121))
key(1); time.sleep(0.6)
check("downloads row menu: Esc closes it", not accent(950, 149) and not accent(950, 121))
key(1); time.sleep(0.5)
# ---- tab menu from the sidebar
click(80, 82, 273); time.sleep(1.0)
check("tab menu: first row is the accent row", accent(150, 103))
key(108); time.sleep(0.4); shot("ui-keys-2-tabmenu")
check("tab menu: Down moves the accent to the second row", accent(150, 131) and not accent(150, 103))
key(1); time.sleep(0.6)
check("tab menu: Esc closes it", not accent(150, 131))
# ---- sidebar hover shows the close glyph (a hovered row)
hover(110, 82); shot("ui-hover-3-sidebar")
check("sidebar: a hovered row shows its close glyph", any(pixel(x, 82) != pixel(100, 82) for x in range(190, 202)))
settle()
print(f"RESULT {sum(res)}/{len(res)}")
