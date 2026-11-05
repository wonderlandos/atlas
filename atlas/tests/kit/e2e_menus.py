#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""e2e_menus.py: the Kit popovers on slot 8 with real pointer clicks (vinput): app menu (row runs its command,
zoom row keeps the popover open and changes the page zoom, submenu with a back row), privacy popover (its link
opens Settings), downloads row menu (right click on a row of the downloads list). Run ON the forge after
`run.sh wol-light OUT --atlas-kit-chrome URL URL` with the download server of the lane on 127.0.0.1:8099.
Prints one line per check (PASS/FAIL), screenshots go to the shots dir."""
import json, subprocess, sys, time
K = "/scratch/atlas-slot8/kitrun"; S = "/scratch/atlas-slot8/shots"; T = sys.argv[1] if len(sys.argv) > 1 else "wol-light"
PORT = "9348"
ENV = {"XDG_RUNTIME_DIR": f"{K}/run-{T}", "WAYLAND_DISPLAY": "wayland-0", "PATH": "/usr/bin:/bin"}
def cdp(*a):
    return subprocess.run(["python3", "-I", f"{K}/cdp.py", PORT, *a], capture_output=True, text=True, timeout=30).stdout.strip()
def tabs():
    return json.loads(cdp("tabs"))
def click(x, y, button=272):
    subprocess.run([f"{K}/in.sh", T, "click", str(x), str(y), str(button)], check=True, capture_output=True)
def key(k):
    subprocess.run([f"{K}/in.sh", T, "key", str(k)], check=True, capture_output=True)
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

MENU = (1262, 17); PRIVACY = (1178, 17); DOWNLOADS = (1234, 17)
n0 = len(tabs())
# ---- app menu: rows come from the engine's model, a row runs its command
click(*MENU); time.sleep(0.8); shot("ui-menu-1-open")
check("app menu opens as a Kit popover (first row is the accent row)", accent(1150, 52))
click(1223, 350); time.sleep(0.6)
first = tabs()[0]["id"]
check("zoom plus changes the page zoom", cdp("eval", first, "window.devicePixelRatio").startswith("1.1"))
check("zoom row keeps the popover open (menu row still there)", accent(1150, 52))
shot("ui-menu-2-zoom")
click(1100, 173); time.sleep(0.8); shot("ui-menu-3-submenu")
check("History opens a submenu with a back row", accent(1150, 52) and not accent(1150, 80))
click(1160, 52); time.sleep(0.8)
check("back row returns to the first menu", accent(1150, 52) and not accent(1150, 80))
click(1100, 52); ms, ok = wait(lambda: len(tabs()) == n0 + 1)
check("New tab row runs IDC_NEW_TAB", ok is not None, f"tabs {n0} -> {len(tabs())}")
time.sleep(1.0)
# ---- privacy popover
click(*PRIVACY); time.sleep(1.0); shot("ui-privacy-1-open")
check("privacy popover opens (accent row 'Privacy settings')", accent(1000, 437))
click(1000, 437); ms, ok = wait(lambda: any("settings/privacy" in t["url"] for t in tabs()))
check("Privacy settings link opens the settings page", ok is not None)
time.sleep(1.0)
# ---- downloads row menu
cdp("new", "http://127.0.0.1:8099/porto-walks.pdf"); time.sleep(3)
click(*DOWNLOADS); time.sleep(1.5); shot("ui-downloads-1-list")
click(1090, 100, 273); time.sleep(1.2); shot("ui-downloads-2-rowmenu")
check("row menu opens as a Kit popover beside the list (list stays open)", accent(950, 121))
click(950, 177)  # "Always open with system viewer": runs the engine's command, closes the popover
time.sleep(1.0); shot("ui-downloads-3-after")
check("a row of the row menu closes the popover", not accent(950, 121))
print(f"RESULT {sum(res)}/{len(res)}")
