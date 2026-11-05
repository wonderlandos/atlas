#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""measure_chrome.py: the numbers of the window against tokens.md, from grim screenshots.

  python3 -I atlas/tests/measure_chrome.py SHOTDIR --rows wol-light.json [--json OUT.json]

SHOTDIR holds idle.png, focus.png (after Super L), load.png (a throttled load), find.png (after
Super F), download.png (mid-download); a missing one skips its rows. The shots are 8-bit RGB(A) at
scale 1. --rows is `<theme id>.json` written by `WOL_DUMP_GTK3=<dir> cargo test -p core
dump_for_the_sampler`: the colours the theme gives the engine, flattened. Every row prints "ok" or
"DEVIATION" with the expected and the measured value; exit 1 when a row fails. Standard library only.

Rows (tokens.md sections 2, 4, 8; the ATLAS-CHROME acceptance): toolbar 36 +-1, toolbar fill vs
glass, omnibox 24 +-1, centred +-8, width <= 560, fill vs control, border, focus ring accent
(dE <= 4, 2 px outside), focused fill vs card, load bar 2 px accent from the left edge, find bar 36
+-1 px of glass, downloads ring in accent.
"""
import argparse
import json
import math
import os
import struct
import sys
import zlib


def load_png(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("%s: not a PNG" % path)
    pos, idat = 8, b""
    w = h = bpp = 0
    while pos < len(data):
        ln, typ = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + ln]
        pos += 12 + ln
        if typ == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", body[:10])
            if depth != 8 or ctype not in (2, 6):
                raise SystemExit("only 8-bit RGB/RGBA PNG")
            bpp = 3 if ctype == 2 else 4
        elif typ == b"IDAT":
            idat += body
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev, i = [], bytearray(stride), 0
    for _ in range(h):
        f = raw[i]
        cur = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = cur[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                cur[x] = (cur[x] + a) & 255
            elif f == 2:
                cur[x] = (cur[x] + b) & 255
            elif f == 3:
                cur[x] = (cur[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                cur[x] = (cur[x] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        rows.append(cur)
        prev = cur
    return w, h, bpp, rows


def parse_colour(s, over=None):
    """'#rrggbb' or 'rgba(r,g,b,a)' -> (r,g,b), flattened over `over` when it has alpha."""
    s = s.strip()
    if s.startswith("#"):
        s = s[1:]
        if len(s) == 3:
            s = "".join(c * 2 for c in s)
        return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4))
    v = s[s.index("(") + 1:s.rindex(")")].split(",")
    r, g, b = (float(x) for x in v[:3])
    a = float(v[3]) if len(v) > 3 else 1.0
    base = over or (255, 255, 255)
    return tuple(round(a * c + (1 - a) * o) for c, o in zip((r, g, b), base))


def lab(c):
    def f(x):
        x /= 255.0
        return x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4
    r, g, b = (f(v) for v in c)
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883
    t = lambda v: v ** (1 / 3) if v > 0.008856 else 7.787 * v + 16 / 116
    fx, fy, fz = t(x), t(y), t(z)
    return 116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)


def de(a, b):
    return math.dist(lab(a), lab(b))


class Shot:
    def __init__(self, path):
        self.w, self.h, self.bpp, self.rows = load_png(path)

    def px(self, x, y):
        o = x * self.bpp
        r = self.rows[y]
        return r[o], r[o + 1], r[o + 2]

    def run_down(self, x, y0, colour, tol, limit=400):
        """Length of the vertical run starting at y0 of pixels within dE `tol` of colour."""
        n = 0
        while y0 + n < min(self.h, y0 + limit) and de(self.px(x, y0 + n), colour) <= tol:
            n += 1
        return n

    def run_across(self, y, x0, colour, tol):
        """(start, end) of the horizontal run of `colour` containing x0 on row y."""
        a = x0
        while a > 0 and de(self.px(a - 1, y), colour) <= tol:
            a -= 1
        b = x0
        while b < self.w - 1 and de(self.px(b + 1, y), colour) <= tol:
            b += 1
        return a, b


def hexrgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def expected_from_dump(path):
    """Colours the theme gives the engine, from `WOL_DUMP_GTK3=<dir> cargo test -p core dump_for_the_sampler`
    (<dir>/<theme id>.json): the flattened colour of every engine row."""
    rows = {r["engine"]: hexrgb(r["expect"]) for r in json.load(open(path))["rows"]}
    return {
        "surface": rows["kColorPrimaryBackground"],
        "sidebar": rows["kColorFrameActive"],
        "glass": rows["AtlasToolbarBackground"],
        "card": rows["kColorMenuBackground"],
        "control": rows["kColorTextfieldBackground"],
        "accent": rows["kColorAccent"],
        "line": rows["kColorMidground"],
    }


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("shots", help="directory with idle.png, focus.png, load.png, find.png, download.png (any may be missing)")
    ap.add_argument("--rows", required=True, help="<theme id>.json of the theme dump (expected colours)")
    ap.add_argument("--json", help="write the rows here")
    a = ap.parse_args(argv[1:])
    expect = expected_from_dump(a.rows)
    rows = []

    def row(name, want, got, ok):
        rows.append({"row": name, "expected": want, "measured": got, "pass": bool(ok)})
        print("%-9s %-40s expected %-22s measured %s" % ("ok" if ok else "DEVIATION", name, want, got))

    def shot(name):
        p = os.path.join(a.shots, name + ".png")
        return Shot(p) if os.path.exists(p) else None

    idle = shot("idle")
    if idle:
        s = idle
        cx = s.w // 2
        # the toolbar line: the last row of the line colour above the page, scanning the empty toolbar area
        col = cx + 330
        # top of the toolbar: bottom of the tab strip when there is one (a tab at x=100), else 0
        top = 0
        if de(s.px(100, 10), s.px(col, 10)) > 3:
            tab = s.px(100, 10)
            top = max(y for y in range(0, 60) if de(s.px(100, y), tab) <= 3) + 1
        # first row of glass in the empty toolbar area, then the line under it (the first row that is not glass)
        glass_top = next(y for y in range(top, top + 12) if de(s.px(col, y), expect["glass"]) <= 3)
        y_line = next(y for y in range(glass_top + 20, 140) if de(s.px(col, y), expect["glass"]) > 3)
        row("toolbar height", "36 +-1 px", y_line - top, abs(y_line - top - 36) <= 1)
        row("toolbar fill vs glass", "dE <= 3", "%.1f" % de(s.px(col, top + 6), expect["glass"]), de(s.px(col, top + 6), expect["glass"]) <= 3)
        ys = [y for y in range(glass_top, y_line) if de(s.px(cx, y), expect["glass"]) > 3]
        y0, y1 = min(ys), max(ys)
        row("omnibox height", "24 +-1 px", y1 - y0 + 1, abs(y1 - y0 + 1 - 24) <= 1)
        mid = (y0 + y1) // 2
        xs = [x for x in range(s.w) if de(s.px(x, y0 + 3), expect["glass"]) > 3 and 200 < x < s.w - 200]
        x0, x1 = min(xs), max(xs)
        row("omnibox centred", "+-8 px of %d" % cx, "%.1f" % ((x0 + x1) / 2.0), abs((x0 + x1) / 2.0 - cx) <= 8)
        row("omnibox width", "<= 560 px", x1 - x0 + 1, (x1 - x0 + 1) <= 560)
        c = s.px(cx - 100, y0 + 3)
        row("omnibox fill vs control", "dE <= 3", "%.1f" % de(c, expect["control"]), de(c, expect["control"]) <= 3)
        row("omnibox border (line over control)", "1 px, darker than fill", "%s" % ("%d px" % sum(1 for y in (y0, y1) if de(s.px(cx - 100, y), expect["control"]) > 2)), de(s.px(cx - 100, y0), expect["control"]) > 2)
        geom = (y0, y1, x0, x1, cx)
    else:
        geom = None
    foc = shot("focus")
    if foc and geom:
        y0, y1, x0, x1, cx = geom
        ring = [foc.px(cx, y0 - d) for d in (1, 2, 3)]
        n = sum(1 for p in ring if de(p, expect["accent"]) <= 4.0)
        row("focus ring colour, 2 px outside", "accent dE <= 4, 2 px", "%d px, dE %.1f" % (n, de(ring[0], expect["accent"])), n == 2)
        row("focused fill vs card", "dE <= 3", "%.1f" % de(foc.px(cx - 100, y0 + 3), expect["card"]), de(foc.px(cx - 100, y0 + 3), expect["card"]) <= 3)
    ld = shot("load")
    if ld and geom:
        y0, y1, x0, x1, cx = geom
        bar = [y for y in range(y1 - 4, y1 + 2) if de(ld.px(x0 + 12, y), expect["accent"]) <= 4.0]
        run = 0
        while x0 + 12 + run < x1 and de(ld.px(x0 + 12 + run, y1 - 1), expect["accent"]) <= 4.0:
            run += 1
        row("load bar height (accent)", "2 px", len(bar), len(bar) == 2)
        row("load bar follows the value", "a partial run from the left edge", "%d of %d px (%d%%)" % (run + 12, x1 - x0, 100 * (run + 12) // max(1, x1 - x0)), 12 < run + 12 < (x1 - x0))
    fnd = shot("find")
    if fnd and geom:
        y0, y1, x0, x1, cx = geom
        x = fnd.w - 150  # between the field and the buttons, inside the bar
        y_line = y1 + 6
        ys = [y for y in range(y_line, y_line + 90) if de(fnd.px(x, y), expect["glass"]) <= 3]
        row("find bar height (glass)", "36 +-1 px", len(ys), abs(len(ys) - 36) <= 1)
    dl = shot("download")
    if dl and geom:
        n = sum(1 for y in range(4, 32) for x in range(dl.w - 70, dl.w - 30) if de(dl.px(x, y), expect["accent"]) <= 8.0)
        row("downloads ring visible (accent px)", ">= 20 px", n, n >= 20)
    if a.json:
        json.dump(rows, open(a.json, "w"), indent=1)
    return 0 if all(r["pass"] for r in rows) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
