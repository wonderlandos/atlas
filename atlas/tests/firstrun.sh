#!/usr/bin/env bash
# firstrun.sh: tests of the first-run page and its wiring (no engine tree needed).
#   1. the page's static checks (components/atlas_pages/setup/tools/check.py);
#   2. the choices logic against a fake environment (C++, compiled here with the mini gtest);
#   3. the engine adapter against a fake page bridge (node --test);
#   4. the page with the real adapter in a headless browser, keyboard only (needs
#      PLAYWRIGHT_CORE and CHROME, else skipped and said so);
#   5. the pieces agree: message names, resource names, the preference name, the series, the
#      dropped bundle.
# The engine unit tests (*AtlasFirstRun*) and the real chrome://setup walk run in a dev slot.
set -u
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
dir="$root/components/atlas_pages/setup"
rc=0
err() { echo "FAIL $*"; rc=1; }
ok() { echo "ok   $*"; }

# 1
if out="$(cd "$dir" && python3 -I tools/check.py 2>&1)"; then ok "page checks: $(echo "$out" | tail -1)"; else echo "$out"; err "tools/check.py"; fi

# 2
bin="$(mktemp -d)/apply_test"
if g++ -std=c++20 -Wall -Wextra -Werror -DATLAS_STANDALONE_TEST -I"$root" \
    "$dir/first_run_apply.cc" "$dir/first_run_apply_unittest.cc" -o "$bin" 2>&1; then
    if out="$("$bin")"; then ok "choices logic: $(echo "$out" | tail -1)"; else echo "$out"; err "first_run_apply_unittest"; fi
else
    err "first_run_apply_unittest does not compile"
fi

# 3
if out="$(cd "$dir" && node --test tests/host_webui.test.mjs 2>&1)"; then
    ok "adapter: $(echo "$out" | grep -E '^# pass' | sed 's/# //') tests"
else
    echo "$out" | tail -40; err "tests/host_webui.test.mjs"
fi

# 4
if [ -n "${PLAYWRIGHT_CORE:-}" ] && [ -n "${CHROME:-}" ]; then
    if out="$(cd "$dir" && node tools/adapter_walk.mjs 2>&1)"; then ok "$out"; else echo "$out"; err "tools/adapter_walk.mjs"; fi
else
    echo "skip adapter walk in a browser (set PLAYWRIGHT_CORE and CHROME)"
fi

# 5
python3 -I - "$root" <<'PY' || rc=1
import re, sys, pathlib
root = pathlib.Path(sys.argv[1])
d = root / "components/atlas_pages/setup"
bad = []
def ok(m): print("ok  ", m)

# message names: adapter == handler header
js = (d / "host_webui.js").read_text()
h = (d / "first_run_handler.h").read_text()
for key, const in (("state", "kGetStateMessage"), ("finish", "kFinishMessage")):
    jsname = re.search(rf"{key}: '([^']+)'", js).group(1)
    cname = re.search(rf'{const}\[\] = "([^"]+)"', h).group(1)
    if jsname != cname:
        bad.append(f"message {key}: adapter '{jsname}' != handler '{cname}'")
ok("message names agree between host_webui.js and first_run_handler.h")

# the settings page's own default-browser messages exist in the engine (names kept in step by hand)
for name in ("requestDefaultBrowserState", "setAsDefaultBrowser", "browser-default-state-changed"):
    if name not in js:
        bad.append(f"adapter does not use {name}")

# setup_resources.grdp: every file exists (file= is relative to the root grd,
# components/atlas_pages, so "setup/<name>"), served as setup/<name> (host_webui.js as host.js),
# the page has an index, and the preview-only files are not in the build
grdp = (d / "setup_resources.grdp").read_text()
incs = re.findall(r'<include name="(IDR_\w+)"\s+file="setup/([^"]+)"\s+resource_path="([^"]+)"', grdp)
if len(incs) != grdp.count("<include "):
    bad.append("setup_resources.grdp has a file= that is not setup/<name>")
for _, f, rp in incs:
    if not (d / f).is_file():
        bad.append(f"setup_resources.grdp lists a missing file {f}")
    want = "setup/" + {"host_webui.js": "host.js"}.get(f, f)
    if rp != want:
        bad.append(f"{f} is served as {rp}, expected {want}")
paths = {rp for _, _, rp in incs}
if "setup/index.html" not in paths:
    bad.append("setup/index.html is not in the page's resources")
if {f for _, f, _ in incs} & {"theme.css", "host.js"}:
    bad.append("a preview-only file (theme.css, host.js) is in the build")
html = (d / "index.html").read_text()
for ref in re.findall(r'(?:src|href)="([a-z]+\.(?:css|js))"', html):
    if "setup/" + ref not in paths and ref != "theme.css":
        bad.append(f"index.html loads {ref}, which the page source does not serve")
ok(f"setup_resources.grdp, served names and the page's references agree ({len(incs)} resources)")

# the preference literal in the component-loader hunk equals the constant
prefs = re.search(r'kTrackerBlocking\[\] = "([^"]+)"', (d / "setup_prefs.h").read_text()).group(1)
p54 = (root / "atlas/patches/ui/0054-firstrun-handler.patch").read_text()
if f'GetBoolean("{prefs}")' not in p54:
    bad.append(f"patch 0054 does not read the preference {prefs}")
ok(f"patch 0054 reads {prefs}")

# the series fragment lists the three patches, and 0053 leaves no reference to the dropped bundle
frag = (root / "atlas/patches/series.d/50-pages.series").read_text().split()
for p in ("ui/0053-firstrun-page.patch", "ui/0054-firstrun-handler.patch", "ui/0055-credits-licence-files.patch"):
    if p not in frag:
        bad.append(f"50-pages.series does not list {p}")
added = "\n".join(l for l in (root / "atlas/patches/ui/0053-firstrun-page.patch").read_text().splitlines() if l.startswith("+") and not l.startswith("+++"))
if "helium_onboarding" in added:
    bad.append("patch 0053 adds a reference to the dropped bundle")
ok("series fragment lists 0053, 0054, 0055; 0053 adds no reference to the dropped bundle")

# the bundle's download stays (the inherited i18n tools read its version), but after 0053 nothing
# in the build reaches it: 0053 takes the bundle out of every BUILD.gn, pak list, resource id and grd
if not re.search(r"^\[onboarding\]", (root / "deps.ini").read_text(), re.M):
    bad.append("deps.ini lost the setup bundle section the i18n tools read")
removed = "\n".join(l for l in (root / "atlas/patches/ui/0053-firstrun-page.patch").read_text().splitlines()
                    if l.startswith("-") and not l.startswith("---"))
for ref in ("//components/helium_onboarding:generated_resources", "helium_onboarding_generated.pak",
            "helium_onboarding_strings.grdp"):
    if ref not in removed:
        bad.append(f"0053 does not take {ref} out of the build")
ok("deps.ini keeps the bundle section; 0053 takes the bundle out of the build")

# no engine include in the pure logic
if re.search(r'#include\s+"(base|chrome|content)/', (d / "first_run_apply.h").read_text() + (d / "first_run_apply.cc").read_text()):
    bad.append("first_run_apply pulls in the engine")

for b in bad:
    print("FAIL", b)
sys.exit(1 if bad else 0)
PY

[ "$rc" -eq 0 ] && echo "ok   first run: all checks passed"
exit "$rc"
