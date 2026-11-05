#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Inline the Atlas badge into index.html as <symbol id="atlas-mark">.

The page is one self-contained document, so the badge is a symbol in its
sprite (no file to fetch, no namespace URL). Re-run when the brand set changes.

usage: python3 -I tools/mark.py <badge.svg> [--check]
  <badge.svg>  e.g. brand/atlas/directions/atlas-b-folio-128.svg (direction B, Folio)
"""
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent.parent
PAGE = HERE / "index.html"
BEGIN, END = "<!-- mark:begin -->", "<!-- mark:end -->"


def symbol(svg: str) -> str:
    m = re.search(r"<svg\b([^>]*)>(.*)</svg>\s*$", svg, re.S)
    if not m:
        raise SystemExit("not an svg document")
    view = re.search(r'viewBox="([^"]+)"', m.group(1)).group(1)
    inner = re.sub(r"<title>.*?</title>", "", m.group(2), flags=re.S)
    # ids are prefixed so nothing else in the page can collide with them
    ids = re.findall(r'\bid="([^"]+)"', inner)
    for i in ids:
        inner = inner.replace(f'id="{i}"', f'id="mark-{i}"').replace(f"url(#{i})", f"url(#mark-{i})")
    return f'<symbol id="atlas-mark" viewBox="{view}">{inner}</symbol>'


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    sym = symbol(pathlib.Path(sys.argv[1]).read_text())
    page = PAGE.read_text()
    head, rest = page.split(BEGIN, 1)
    _, tail = rest.split(END, 1)
    new = f"{head}{BEGIN}{sym}{END}{tail}"
    if "--check" in sys.argv[2:]:
        if new != page:
            print("index.html carries an old badge: run tools/mark.py", file=sys.stderr)
            return 1
        print("index.html badge matches", sys.argv[1])
        return 0
    PAGE.write_text(new)
    print(f"inlined {sys.argv[1]} into {PAGE}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
