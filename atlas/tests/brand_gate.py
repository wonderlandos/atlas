#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Wonderland Doctrine gate for the brand: nothing a person can see names the inherited project.

  brand_gate.py --tree TREE        a source tree after atlas/transform/apply.py: strings, page sources,
                                   product branding, the brand slot, no host of the project
  brand_gate.py --package DIR      an unpacked package (the tarball of the build, or the installed
                                   /opt/atlas): file names, the desktop file, the icons, the text of
                                   the .pak files that are not compressed, hosts in the binary

Both print one line per finding and exit 1 if there is any. What may stay (identifiers, pref keys,
class names, licence texts, data) is listed under "internal" in atlas/tests/transform-counts.json
as {"name": {"why": "...", "paths": [glob...]}}; a finding in a path matching a glob is not reported.
"""

import argparse
import fnmatch
import hashlib
import json
import os
import re
import sys

sys.dont_write_bytecode = True  # the tools are run with -I from a checkout: leave no __pycache__
import xml.etree.ElementTree as xml
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / "atlas" / "transform"))

NAME = re.compile(r"\bHelium\b")
SCHEME = re.compile(r"\bhelium://")
# the product's own words (transform words.py): a private window is "Private", in the English messages
# (a translation keeps its own word) and the strings the product reads out in its internal pages
INCOGNITO = re.compile(r"\b[Ii]ncognito\b")
WORDS_IN_FILES = {
    "components/policy/core/browser/policy_conversions.h": re.compile(r'"Chrome Policies"'),
    "components/webui/chrome_urls/resources/app.html.ts": re.compile(r"List of Chrome URLs"),
    "chrome/browser/resources/net_internals/index.html": re.compile(r"\bchrome://(?:net-export|network)"),
}
HOSTS = re.compile(r"helium\.computer|imput\.net|imputnet|helium-services-are-disabled|services\.helium")

# source files whose text a person can read in the browser
TEXT_EXTS = {".grd", ".grdp", ".xtb", ".html", ".ts", ".js", ".css", ".svelte", ".json", ".desktop", ".mjs"}
# directories of the tree that never reach the package or that are not text of the browser
SKIP = {"out", ".git", "node_modules", "testdata", "test", "tests", "web_tests", "tools", "docs",
        "ash", "chromeos", "remoting", "android", "ios", "mac", "win", "helium_onboarding", ".pc"}


def internal():
    data = json.loads((HERE / "transform-counts.json").read_text(encoding="utf-8"))
    return [g for entry in data.get("internal", {}).values() for g in entry.get("paths", [])]


def allowed(rel, globs):
    return any(fnmatch.fnmatch(rel, g) for g in globs)


def is_comment(line):
    s = line.strip()
    return s.startswith(("//", "/*", "*", "#", "<!--", "-->"))


def visible(node):
    """The text a person reads in a <message>/<translation>: text and tails, children included."""
    parts = [node.text or ""]
    for child in node:
        parts.append(visible(child))
        parts.append(child.tail or "")
    return "".join(parts)


def scan_grit(path, rel, findings):
    try:
        root = xml.fromstring(path.read_text(encoding="utf-8").replace("&#36;", "$"))
    except (xml.ParseError, UnicodeDecodeError):
        findings.append(f"{rel}: not readable as xml")
        return
    for node in list(root.iter("message")) + list(root.iter("translation")):
        text = visible(node)
        if NAME.search(text) or SCHEME.search(text) or HOSTS.search(text):
            findings.append(f"{rel}: {node.get('name') or node.get('id')}: {text.strip()[:100]}")
        elif node.tag == "message" and INCOGNITO.search(text):
            findings.append(f"{rel}: {node.get('name')}: says Incognito, the product says Private: {text.strip()[:80]}")


def scan_tree(tree, findings):
    globs = internal()
    tree = Path(tree)
    for root, dirs, files in os.walk(tree):
        rel_root = Path(root).relative_to(tree).parts
        if rel_root == ("third_party",):
            # the engine's own libraries are not ours to read; the blocker we ship is
            dirs[:] = [d for d in dirs if d == "ublock"]
        else:
            dirs[:] = [d for d in dirs if d not in SKIP]
        for name in files:
            ext = os.path.splitext(name)[1].lower()
            if ext not in TEXT_EXTS:
                continue
            path = Path(root) / name
            rel = str(path.relative_to(tree))
            if allowed(rel, globs):
                continue
            if ext in {".grd", ".grdp", ".xtb"}:
                scan_grit(path, rel, findings)
                continue
            try:
                text = path.read_text(encoding="utf-8")
            except (UnicodeDecodeError, OSError):
                continue
            for n, line in enumerate(text.splitlines(), 1):
                if ext == ".json":
                    line = line.split("//")[0]
                if ext in {".ts", ".js", ".css", ".mjs", ".html"} and is_comment(line):
                    continue
                if NAME.search(line) or SCHEME.search(line) or HOSTS.search(line):
                    findings.append(f"{rel}:{n}: {line.strip()[:120]}")
    for rel, pattern in WORDS_IN_FILES.items():
        f = tree / rel
        if f.is_file() and pattern.search(f.read_text(encoding="utf-8", errors="replace")):
            findings.append(f"{rel}: still says the engine's words (transform words.py)")
    # the blocker list whose licence does not fit leaves the package
    assets = tree / "third_party/ublock/assets/assets.json"
    if assets.is_file() and "plowe" in assets.read_text(encoding="utf-8"):
        findings.append("third_party/ublock/assets/assets.json: still lists the noncommercial server list")
    if (tree / "third_party/ublock/assets/thirdparties/pgl.yoyo.org").exists():
        findings.append("third_party/ublock/assets/thirdparties/pgl.yoyo.org: the noncommercial list files are still in the tree")
    # product branding file
    branding = (tree / "chrome/app/theme/chromium/BRANDING").read_text(encoding="utf-8")
    if re.search(r"(?i)helium|imput", branding.replace("MAC_TEAM_ID=S4Q33XPHB4", "")):
        findings.append("chrome/app/theme/chromium/BRANDING: still names the inherited project")
    check_slot(tree, findings)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def old_hashes():
    out = {}
    for line in (HERE / "old-brand.sha256").read_text(encoding="utf-8").splitlines():
        h, name = line.split(None, 1)
        out[h] = name.strip()
    return out


def check_slot(tree, findings):
    """Every destination of the brand slot holds the Atlas file; no picture of the old brand is left."""
    slot = ROOT / "atlas" / "resources"
    for line in (slot / "atlas_resources.txt").read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        src, dst = line.split()
        if not (Path(tree) / dst).is_file() or sha(Path(tree) / dst) != sha(slot / src):
            findings.append(f"{dst}: is not the Atlas brand file {src}")
    old = old_hashes()
    for ext in ("*.png", "*.svg", "*.icon"):
        for root, dirs, files in os.walk(tree):
            dirs[:] = [d for d in dirs if d not in {"out", ".git", "node_modules", "sysroot"}]
            for name in files:
                if name.endswith(ext[1:]):
                    p = Path(root) / name
                    if p.stat().st_size < 200_000 and sha(p) in old:
                        findings.append(f"{p.relative_to(tree)}: byte for byte a picture of the old brand ({old[sha(p)]})")


def scan_package(pkg, findings):
    globs = internal()
    pkg = Path(pkg)
    old = old_hashes()
    for root, _, files in os.walk(pkg):
        for name in files:
            path = Path(root) / name
            rel = str(path.relative_to(pkg))
            if allowed(rel, globs):
                continue
            if re.search(r"(?i)helium", rel):
                findings.append(f"{rel}: the file name names the inherited project")
            size = path.stat().st_size
            if size < 200_000 and name.endswith((".png", ".svg", ".icon")) and sha(path) in old:
                findings.append(f"{rel}: byte for byte a picture of the old brand")
            if name.endswith(".desktop"):
                for n, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                    if re.search(r"(?i)helium|imput", line):
                        findings.append(f"{rel}:{n}: {line.strip()[:100]}")
            elif name.endswith(".pak") or name == "atlas" or name.endswith((".so", ".json", ".html", ".js")):
                data = path.read_bytes()
                for pat, what in ((rb"\bHelium\b", "name"), (rb"helium://", "scheme"),
                                  (rb"helium\.computer|imput\.net|imputnet|services\.helium|helium-services-are-disabled", "host")):
                    m = re.search(pat, data)
                    if m:
                        a = max(0, m.start() - 30)
                        findings.append(f"{rel}: {what} in the file: ...{data[a:m.end() + 30].decode('utf-8', 'replace')!r}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--tree")
    g.add_argument("--package")
    a = p.parse_args()
    findings = []
    if a.tree:
        scan_tree(a.tree, findings)
    else:
        scan_package(a.package, findings)
    for f in findings[:200]:
        print("FAIL " + f)
    if len(findings) > 200:
        print(f"FAIL ... and {len(findings) - 200} more")
    if not findings:
        print("ok   brand gate: nothing names the inherited project")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
