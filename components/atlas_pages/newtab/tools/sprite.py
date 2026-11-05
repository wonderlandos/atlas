#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Inline the glyphs of the new tab frame into index.html as <symbol> elements.

The glyphs (16 px grid, filled outlines) are the theme's symbolic icons as the brand frame draws
them: web-browser, mail-unread, folder-download, media-playback-start, starred,
preferences-system (the six tiles), text-x-generic (a page to pick up) and system-search.
The page is one self-contained document, so they are symbols in its sprite. Re-run when the
frame changes.

usage: python3 -I tools/sprite.py <atlas-newtab.svg> [--check]
  <atlas-newtab.svg>  brand/atlas/chrome/atlas-newtab.svg
"""
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent.parent
PAGE = HERE / "index.html"
BEGIN, END = "<!-- glyphs:begin -->", "<!-- glyphs:end -->"
TILES = ("web", "talk", "files", "media", "store", "system")


def symbols(svg: str) -> str:
    tail = svg[svg.index(">Pinned<"):]
    tiles = re.findall(
        r'<rect x="[\d.]+" y="434" width="32" height="32" rx="8" fill="#\w+" />'
        r'<g transform="translate\([\d. ]+\) scale\(1\)" fill="#FFFFFF"><path d="([^"]+)"/></g>',
        tail,
    )
    doc = re.search(r'translate\(438\.0 582\) scale\(1\)" fill="#7C5CFF"><path d="([^"]+)"', tail)
    search = re.search(r'translate\([\d.]+ 332\) scale\(1\)" fill="#686575"><path d="([^"]+)"', svg)
    if len(tiles) != len(TILES) or not doc or not search:
        raise SystemExit("the frame does not have the six tiles, the page glyph and the search glyph")
    out = [f'<symbol id="g-{n}" viewBox="0 0 16 16"><path d="{d}"/></symbol>' for n, d in zip(TILES, tiles)]
    out.append(f'<symbol id="g-doc" viewBox="0 0 16 16"><path d="{doc.group(1)}"/></symbol>')
    out.append(f'<symbol id="g-search" viewBox="0 0 16 16"><path d="{search.group(1)}"/></symbol>')
    return "\n".join(out)


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    sym = symbols(pathlib.Path(sys.argv[1]).read_text())
    page = PAGE.read_text()
    head, rest = page.split(BEGIN, 1)
    _, tail = rest.split(END, 1)
    new = f"{head}{BEGIN}\n{sym}\n{END}{tail}"
    if "--check" in sys.argv[2:]:
        if new != page:
            print("index.html carries old glyphs: run tools/sprite.py", file=sys.stderr)
            return 1
        print("index.html glyphs match", sys.argv[1])
        return 0
    PAGE.write_text(new)
    print(f"inlined the glyphs of {sys.argv[1]} into {PAGE}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
