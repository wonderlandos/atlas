#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Write atlas/patches/ui/0051-newtab-handler.patch: the new files of the new tab page.

The page, its resources, the logic with no //chrome dependency and the handler are new files, so
the patch has no context to rebase: it is generated from the files of this clone, never edited.
The preview-only files (theme.css, categories.css, host.js, tools, testdata, README) are not
part of it.

usage: python3 -I tools/mkpatch.py <clone-root> [--check]
"""
import difflib
import pathlib
import sys

NEWTAB = "components/atlas_pages/newtab/"
UI = "chrome/browser/ui/atlas/newtab/"
FILES = [
    NEWTAB + n for n in (
        "BUILD.gn", "index.html", "newtab.css", "newtab.js", "host.engine.js",
        "newtab_resources.grd", "newtab_categories.h", "newtab_categories.cc",
        "newtab_state.h", "newtab_state.cc", "newtab_state_unittest.cc",
    )
] + [
    UI + n for n in (
        "BUILD.gn", "newtab_handler.h", "newtab_handler.cc", "newtab_handler_unittest.cc",
        "newtab_prefs.h", "newtab_prefs.cc", "newtab_ui.h", "newtab_ui.cc",
    )
]

HEADER = """Atlas overlay patch 0051: ui/newtab-handler
Why: the new tab page of Atlas (reel scene R02b): the page files and their resources, the logic with no //chrome dependency (host category table, pinned list, state sent to the page) and the handler that answers the page (pinned pages, pages to pick up from the tab restore list, open, search or address, pin, unpin). All new files, so no rebase cost; the only hunk in an engine file is 0050. A lower tier does not exist: the engine's own page cannot be restyled into this design (the inherited clean-new-tab-page patch only removes parts of it).
Files: components/atlas_pages/newtab/** (new), chrome/browser/ui/atlas/newtab/** (new)
Test filter: *AtlasNewTab*
Rebase risk: L (new files only; the engine APIs they use are listed in the test filter's tests)
Release: 0.13
License: BSD-3-Clause (new files)
Verified at: 156.0.8078.79, NOT built yet (written against the base tree; ATLAS-NTP to build in a dev slot)

"""


def render(root: pathlib.Path) -> str:
    out = [HEADER]
    for rel in FILES:
        text = (root / rel).read_text().splitlines(keepends=True)
        if text and not text[-1].endswith("\n"):
            text[-1] += "\n"
        out.extend(difflib.unified_diff([], text, "/dev/null", "b/" + rel, n=0))
    return "".join(out).replace("--- /dev/null\n+++ b/", "--- /dev/null\n+++ b/")


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    root = pathlib.Path(sys.argv[1])
    target = root / "atlas/patches/ui/0051-newtab-handler.patch"
    text = render(root)
    if "--check" in sys.argv[2:]:
        if not target.exists() or target.read_text() != text:
            print("0051-newtab-handler.patch is stale: run tools/mkpatch.py", file=sys.stderr)
            return 1
        print("0051-newtab-handler.patch matches the files")
        return 0
    target.write_text(text)
    print(f"wrote {target} ({len(FILES)} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
