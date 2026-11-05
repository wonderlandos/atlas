#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""e2e_sidebar.py: the Kit sidebar beyond select/collapse (e2e.py): close glyph on hover, middle click, tab context
menu (Kit popover), keyboard (Down + Enter), drag to reorder. Run ON the forge after
`run.sh wol-light OUT --atlas-kit-chrome URL-A URL-B URL-C URL-D` (titles Alpha, Bravo, Charlie, Delta).
One line per check (PASS/FAIL); screenshots go to the shots dir."""
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
def move(x, y):
    subprocess.run([f"{K}/in.sh", T, "move", str(x), str(y)], check=True, capture_output=True)
def drag(x, y, x2, y2):
    subprocess.run([f"{K}/in.sh", T, "drag", str(x), str(y), str(x2), str(y2)], check=True, capture_output=True)
def key(*k):
    # the virtual keyboard drops the first key of every call: a throw-away key (evdev 194, F24) goes first
    subprocess.run([f"{K}/in.sh", T, "key", "194", *map(str, k)], check=True, capture_output=True)
def shot(name):
    subprocess.run([f"{K}/shot.sh", T, f"{S}/{name}.png"], check=True)
def pixel(x, y):
    out = subprocess.run(["grim", "-g", f"{x},{y} 1x1", "-t", "ppm", "-"], capture_output=True, env=ENV).stdout
    return tuple(out[-3:])
def accent(x, y):
    r, g, b = pixel(x, y)
    return b > 200 and r < 150 and g < 120
def visible():
    for t in tabs():
        if cdp("eval", t["id"], "document.visibilityState") == "\"visible\"":
            return t["title"]
    return ""
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

ROW = lambda i: 82 + 28 * i      # rows under the TODAY header
n0 = len(tabs()); check("four tabs at start", n0 == 4, str(n0))
click(80, ROW(0)); time.sleep(0.6)
check("click selects Alpha", visible() == "Alpha", visible())
# ---- keyboard: from the omnibox Tab walks the toolbar's right end into the sidebar rows (focus order), Down moves
# to the next row, Enter activates it
click(640, 17); time.sleep(0.4)
for _ in range(3):
    key(15); time.sleep(0.25)
key(108); time.sleep(0.3); shot("ui-sidebar-0-keys"); key(28)
ms, ok = wait(lambda: visible() == "Bravo")
check("Tab x3 from the omnibox reaches a sidebar row; Down + Enter activates the next one", ok is not None, visible())
# ---- drag Alpha (row 0) below Delta (row 3): the first row is now Bravo
drag(80, ROW(0), 80, ROW(3) + 20); time.sleep(1.0); shot("ui-sidebar-2-dragged")
click(80, ROW(0)); time.sleep(0.6)
check("drag Alpha to the end: first row is now Bravo", visible() == "Bravo", visible())
click(80, ROW(3)); time.sleep(0.6)
check("... and the last row is Alpha", visible() == "Alpha", visible())
# ---- close glyph on hover
move(80, ROW(1)); time.sleep(0.4); shot("ui-sidebar-3-hover")
n = len(tabs()); click(196, ROW(1))   # glyph at the right end of Charlie's row
ms, ok = wait(lambda: len(tabs()) == n - 1)
check("close glyph on a hovered row closes that tab", ok is not None, f"tabs {n} -> {len(tabs())}")
time.sleep(0.5)
# ---- middle click closes
n = len(tabs()); click(80, ROW(1), 274)
ms, ok = wait(lambda: len(tabs()) == n - 1)
check("middle click closes the tab", ok is not None, f"tabs {n} -> {len(tabs())}")
time.sleep(0.5)
# ---- context menu: Kit popover with the engine's tab commands; its first row duplicates/adds
click(80, ROW(0), 273); time.sleep(1.0); shot("ui-sidebar-4-menu")
check("right click opens the tab menu as a Kit popover (accent row)", accent(100, ROW(0) + 22) or accent(100, ROW(0) + 20))
key(1); time.sleep(0.5)
# ---- groups: a tab goes into a new group (engine editor), the header folds and unfolds it
shot("ui-sidebar-5-before-group")
n = len(tabs())
click(80, ROW(1), 273); time.sleep(0.8); click(150, ROW(1) + 77); time.sleep(1.2)   # "Add tab to new group"
shot("ui-sidebar-6-group-editor"); key(1); time.sleep(1.0)                        # Esc: keep the default name
shot("ui-sidebar-7-group")
# the group header sits under the first tab row; its dot is the group colour (not the sidebar fill)
r, g, b = pixel(20, 107)
check("group header has a colour dot", max(r, g, b) - min(r, g, b) > 60, str((r, g, b)))
pill = pixel(150, 134)                       # the active tab's pill, under the header
click(80, 108); time.sleep(0.8); shot("ui-sidebar-8-group-folded")
check("click on the group header folds it (the pill is gone: one summary row)", pixel(150, 134) != pill, f"{pill} -> {pixel(150, 134)}")
click(80, 108); time.sleep(0.8); shot("ui-sidebar-9-group-unfolded")
check("a second click unfolds it", pixel(150, 134) == pill)
print(f"RESULT {sum(res)}/{len(res)}")
