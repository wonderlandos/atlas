#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""trace.py THEME X Y [LABEL]: records a browser trace around one real click at (X, Y) on the slot 8 window and prints
the time the browser spent handling the click and the time from the click to the next frame presented (ms).
Uses cdp.py's WebSocket client next to it; standard library only."""
import importlib.util, json, os, subprocess, sys, time, urllib.request

K = "/scratch/atlas-slot8/kitrun"
spec = importlib.util.spec_from_file_location("cdp", os.path.join(K, "cdp.py"))
cdp = importlib.util.module_from_spec(spec)
sys.argv = [sys.argv[0], "9348"] + sys.argv[1:]
spec.loader.exec_module(cdp)
theme, x, y = sys.argv[2], sys.argv[3], sys.argv[4]
label = sys.argv[5] if len(sys.argv) > 5 else "click"
ver = json.loads(urllib.request.urlopen("http://127.0.0.1:9348/json/version", timeout=10).read())
ws = cdp.WS(ver["webSocketDebuggerUrl"])
cats = ["input", "ui", "views", "viz", "cc", "benchmark", "toplevel", "browser", "latencyInfo"]
ws.call("Tracing.start", {"traceConfig": {"includedCategories": cats, "recordMode": "recordAsMuchAsPossible"},
                          "transferMode": "ReportEvents"})
time.sleep(0.8)
subprocess.run([f"{K}/in.sh", theme, "click", x, y], check=True, capture_output=True)
time.sleep(1.2)
ws.n += 1
ws.send({"id": ws.n, "method": "Tracing.end", "params": {}})
events = []
while True:
    m = ws.recv()
    if m.get("method") == "Tracing.dataCollected":
        events += m["params"]["value"]
    elif m.get("method") == "Tracing.tracingComplete":
        break
json.dump(events, open(f"/scratch/atlas-slot8/shots/trace-{label}.json", "w"))
# The click as the browser handled it: the longest event dispatch of the window (press and release run the
# button's action, the tab switch, the sidebar's rebuild). Then the first frame presented after it ended.
disp = [e for e in events if e["name"] == "WindowTreeHostPlatform::DispatchEvent" and e.get("ph") == "X"]
pres = sorted(e["ts"] for e in events if e["name"] == "FramePresented")
if not disp:
    sys.exit(f"{label}: no dispatch in the trace")
d = max(disp, key=lambda e: e.get("dur", 0))
end = d["ts"] + d.get("dur", 0)
nxt = [p for p in pres if p >= end]
print(f"{label}: dispatch {d.get('dur', 0) / 1000:.1f} ms; click -> frame presented "
      f"{(nxt[0] - d['ts']) / 1000:.1f} ms" if nxt else f"{label}: dispatch {d.get('dur', 0) / 1000:.1f} ms; no frame after")
