#!/usr/bin/env python3
# Copyright 2026 The Atlas Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""svg2icon.py SVG... : writes the engine's .icon form of WolOS symbolic glyphs (16 px grid) next to this
file, one kit_<name>.icon each. The glyphs use absolute M, L, A and Z only (the design repo's generator
writes them so); anything else is refused, so a changed glyph never converts silently wrong."""
import os, re, sys

HEADER = """// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The WolOS symbolic glyph {name} (16 px grid), converted by svg2icon.py.

CANVAS_DIMENSIONS, 16,
"""


def num(s):
    v = float(s)
    if v == int(v):
        return str(int(v))
    return repr(v).rstrip("0") + "f"


def convert(d):
    toks = re.findall(r"[MLAZ]|-?\d*\.?\d+(?:e-?\d+)?", d)
    out, i = [], 0
    while i < len(toks):
        c = toks[i]
        if c == "M":
            # Each closed shape is a path of its own, so crossing strokes (the plus) cannot cancel
            # each other where they overlap.
            if out:
                out.append("NEW_PATH,")
            out.append(f"MOVE_TO, {num(toks[i+1])}, {num(toks[i+2])},"); i += 3
        elif c == "L":
            out.append(f"LINE_TO, {num(toks[i+1])}, {num(toks[i+2])},"); i += 3
        elif c == "A":
            a = toks[i+1:i+8]
            out.append(f"ARC_TO, {num(a[0])}, {num(a[1])}, {num(a[2])}, {int(float(a[3]))}, {int(float(a[4]))}, {num(a[5])}, {num(a[6])},"); i += 8
        elif c == "Z":
            out.append("CLOSE,"); i += 1
        else:
            sys.exit(f"svg2icon: unexpected token {c!r}")
    return out


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    for path in sys.argv[1:]:
        text = open(path).read()
        if re.search(r"<path[^>]*d=\"[^\"]*[mlhvcsqtaHVCSQT]", text):
            sys.exit(f"svg2icon: {path} uses path commands other than M L A Z")
        name = os.path.basename(path).replace("-symbolic.svg", "")
        lines = []
        for d in re.findall(r"<path[^>]*\sd=\"([^\"]+)\"", text):
            lines += convert(d)
        out = os.path.join(here, "kit_" + name.replace("-", "_") + ".icon")
        with open(out, "w") as f:
            f.write(HEADER.format(name=name) + "\n".join(lines) + "\n")
        print(out)


if __name__ == "__main__":
    main()
