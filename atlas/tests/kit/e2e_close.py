#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""e2e_close.py: closing windows must not crash (regression for the BookmarkButton use-after-free, ATLAS-E2E findings
10075-10077: a ScopedObservation on the tab's BookmarkTabHelper outlived the tab, every window close died with SIGSEGV,
flag on or off). Run ON the forge after `run.sh wol-light OUT [--atlas-kit-chrome] URL` (both modes count):
Ctrl+N then Ctrl+Shift+W closes the second window, then Ctrl+W on the last tab quits; the browser's log must hold no
fatal signal and the quit must be clean."""
import json, subprocess, sys, time
K = "/scratch/atlas-slot8/kitrun"; LOG = "/scratch/atlas-slot8/shots/run.log"; T = sys.argv[1] if len(sys.argv) > 1 else "wol-light"
PORT = "9348"
def cdp(*a):
    r = subprocess.run(["python3", "-I", f"{K}/cdp.py", PORT, *a], capture_output=True, text=True, timeout=30)
    return r.stdout.strip()
def tabs():
    try:
        return json.loads(cdp("tabs"))
    except ValueError:
        return None
def key(*codes):   # a throw-away key first: the virtual keyboard drops the first of every call
    subprocess.run([f"{K}/in.sh", T, "key", "194", *map(str, codes)], check=True, capture_output=True)
def crashed():
    return b"Received signal" in open(LOG, "rb").read()
res = []
def check(name, ok, extra=""):
    res.append(ok); print(("PASS " if ok else "FAIL ") + name + (" " + extra if extra else ""), flush=True)

check("one tab at start", len(tabs() or []) == 1)
key("+29", 49, "-29"); time.sleep(2.5)     # Ctrl+N: a second window
check("Ctrl+N opens a second window", len(tabs() or []) == 2, str(len(tabs() or [])))
key("+29", "+42", 17, "-42", "-29"); time.sleep(2.5)  # Ctrl+Shift+W: closes it
t = tabs()
check("Ctrl+Shift+W closes the second window and the browser lives", t is not None and len(t) == 1, str(None if t is None else len(t)))
check("... with no fatal signal in the log", not crashed())
key("+29", 17, "-29"); time.sleep(4.0)           # Ctrl+W on the last tab: quits
check("Ctrl+W on the last tab quits the browser (no answer on the debugging port)", tabs() is None)
check("... cleanly: no fatal signal in the log", not crashed())
print(f"RESULT {sum(res)}/{len(res)}")
