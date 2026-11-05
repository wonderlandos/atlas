#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Regenerate (or check) the Atlas brand slot from the design repository.

The art is the Folio mark, direction B of the Atlas identity, drawn by the design team as three SVG
files, one per size class (brand/atlas/directions/atlas-b-folio-{48,128,512}.svg in the design
repository). Every file below is rendered from those with rsvg-convert, the renderer GTK loads the
icons with; nothing is drawn by hand and nothing is scaled up from a smaller picture.

  regenerate.py --design DIR [--write]    render every file of the slot into atlas/resources
  regenerate.py --check [--design DIR]    the slot is complete, every file is the PNG/SVG/ICON it
                                          claims to be and has the recorded sha256 (MANIFEST.sha256);
                                          with --design and rsvg-convert, every PNG is also re-rendered
                                          and compared pixel by pixel

Size classes: 16 to 64 px use the 48 drawing, 128 px the 128 drawing, 256 px and up the 512 drawing.
The slot follows the names of atlas/resources/atlas_resources.txt (the same shape as the file the
inherited project used, so the build copies it with utils/replace_resources.py).
"""

import argparse
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys

sys.dont_write_bytecode = True  # the tools are run with -I from a checkout: leave no __pycache__
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
DIRECTIONS = "brand/atlas/directions"
MANIFEST = HERE / "MANIFEST.sha256"
VERSION_FILE = HERE / "RENDERER"

# (file, kind, width, height)
PNG_SIZES = [16, 24, 32, 48, 64, 128, 256]
# The new tab page's favicon is a monochrome glyph, not the mark: the engine masks it with the tab's text
# colour (favicon::ShouldThemifyFaviconForEntry), so the coloured mark would become a filled square in the
# sidebar. The glyph is the WolOS symbolic "tab-new" (design/icons/symbolic/tab-new-symbolic.svg of the OS
# repository), kept in glyphs/ and rendered here in black like every other masked favicon.
FAVICONS_BRANDED = {"favicon_ntp_16.png": 16, "favicon_ntp_32.png": 32}
NTP_GLYPH = HERE / "glyphs" / "tab-new-symbolic.svg"


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def rsvg(svg_text, w, h, out):
    p = subprocess.run(["rsvg-convert", "-w", str(w), "-h", str(h), "-f", "png", "-o", str(out), "/dev/stdin"],
                       input=svg_text.encode("utf-8"), check=False, capture_output=True)
    if p.returncode:
        sys.exit(f"rsvg-convert failed for {out}: {p.stderr.decode()}")


def read_source(design, size):
    return (Path(design) / DIRECTIONS / f"atlas-b-folio-{size}.svg").read_text(encoding="utf-8")


def class_for(px):
    return 48 if px <= 64 else 128 if px <= 128 else 512


def inner(svg):
    """The drawing without the outer <svg> element (defs and shapes)."""
    m = re.match(r"\s*<svg[^>]*>(.*)</svg>\s*$", svg, re.S)
    assert m, "not a plain svg"
    return re.sub(r"<title>.*?</title>|<!--.*?-->", "", m.group(1), flags=re.S)


def wide(design, h, w):
    """A transparent h-high, w-wide picture with the mark (h x h, 48 drawing up to 64, else 128) in the middle."""
    src = inner(read_source(design, class_for(h)))
    x = (w - h) // 2
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">'
            f'<svg x="{x}" y="0" width="{h}" height="{h}" viewBox="0 0 1024 1024">{src}</svg></svg>')


# --- the glyph, read from the drawing -------------------------------------------------------------------

def glyph(design):
    """Panels (as vertex lists) and the dot (cx, cy, r) of the 128 drawing, in its 1024 units."""
    svg = read_source(design, 128)
    panels = []
    for m in re.finditer(r'<path d="M([\d. ]+)Z" fill="#fff" fill-opacity="([\d.]+)"/>', svg.replace("L", " ")):
        nums = [float(n) for n in m.group(1).split()]
        panels.append(([(nums[i], nums[i + 1]) for i in range(0, len(nums), 2)], float(m.group(2))))
    dot = re.search(r'<path d="M(\d+) (\d+)a(\d+) \d+ 0 1 0 \d+ 0a[^"]*" fill="url\(#b\db\)"', svg)
    assert len(panels) == 3 and dot, "the Folio drawing changed: regenerate.py must be adapted"
    cx, cy, r = int(dot.group(1)) + int(dot.group(3)), int(dot.group(2)), int(dot.group(3))
    return panels, (cx, cy, r)


def top_bottom(panels):
    """y of the top and bottom edge at x (the fold line of the three panels)."""
    tops = [panels[0][0][0], panels[0][0][1], panels[1][0][1], panels[2][0][1]]
    bots = [panels[0][0][3], panels[0][0][2], panels[1][0][2], panels[2][0][2]]

    def at(points, x):
        for (x0, y0), (x1, y1) in zip(points, points[1:]):
            if x0 <= x <= x1:
                return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
        raise ValueError(x)
    return (lambda x: at(tops, x)), (lambda x: at(bots, x))


def gutter_panels(panels, g):
    """The three panels with a gutter of g units between them (for one-colour drawings)."""
    yt, yb = top_bottom(panels)
    xs = [panels[0][0][0][0], panels[0][0][1][0], panels[1][0][1][0], panels[2][0][1][0]]
    out = []
    for i in range(3):
        xa = xs[i] + (g / 2 if i else 0)
        xb = xs[i + 1] - (g / 2 if i < 2 else 0)
        out.append([(xa, yt(xa)), (xb, yt(xb)), (xb, yb(xb)), (xa, yb(xa))])
    return out


def mono_svg(design, size):
    panels, (cx, cy, r) = glyph(design)
    parts = gutter_panels(panels, 16)
    d = ""
    for i, poly in enumerate(parts):
        d += "M" + "L".join(f"{x:.1f} {y:.1f}" for x, y in poly) + "Z"
        if i == 1:  # the dot is a hole in the middle panel
            d += f"M{cx + r} {cy}" + "".join(f"A{r} {r} 0 1 0 {cx - r} {cy}A{r} {r} 0 1 0 {cx + r} {cy}") + "Z"
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="150 150 724 724">'
            f'<path d="{d}" fill="#fff" fill-rule="evenodd"/></svg>')


def f32(v):
    """A number of the vector-icon format: a float literal with the f suffix, or a plain integer (128f is not C++)."""
    s = f"{v:.2f}".rstrip("0").rstrip(".") or "0"
    return s if "." not in s else s + "f"


ICON_HEAD = ("// Copyright 2026 The Atlas Authors\n"
             "// Use of this source code is governed by a BSD-style license that can be\n"
             "// found in the LICENSE file.\n"
             "// Art: the Folio mark, see atlas/resources/LICENSE-brand. Generated by regenerate.py.\n\n")


def icon_mono(design):
    panels, (cx, cy, r) = glyph(design)
    parts = gutter_panels(panels, 16)
    s = 52 / 644
    ox, oy = 2 - 190 * s, 28 - 522 * s
    t = lambda p: (p[0] * s + ox, p[1] * s + oy)
    out = [ICON_HEAD + "CANVAS_DIMENSIONS, 56,", "FILL_RULE_NONZERO,"]
    for i, poly in enumerate(parts):
        q = [t(p) for p in poly]
        out.append(f"MOVE_TO, {f32(q[0][0])}, {f32(q[0][1])},")
        out += [f"LINE_TO, {f32(x)}, {f32(y)}," for x, y in q[1:]]
        out.append("CLOSE,")
        if i == 1:  # hole: the circle wound the other way
            c = t((cx, cy))
            rr = r * s
            k = 0.5522847498 * rr
            out.append(f"MOVE_TO, {f32(c[0] + rr)}, {f32(c[1])},")
            out.append(f"CUBIC_TO, {f32(c[0] + rr)}, {f32(c[1] - k)}, {f32(c[0] + k)}, {f32(c[1] - rr)}, {f32(c[0])}, {f32(c[1] - rr)},")
            out.append(f"CUBIC_TO, {f32(c[0] - k)}, {f32(c[1] - rr)}, {f32(c[0] - rr)}, {f32(c[1] - k)}, {f32(c[0] - rr)}, {f32(c[1])},")
            out.append(f"CUBIC_TO, {f32(c[0] - rr)}, {f32(c[1] + k)}, {f32(c[0] - k)}, {f32(c[1] + rr)}, {f32(c[0])}, {f32(c[1] + rr)},")
            out.append(f"CUBIC_TO, {f32(c[0] + k)}, {f32(c[1] + rr)}, {f32(c[0] + rr)}, {f32(c[1] + k)}, {f32(c[0] + rr)}, {f32(c[1])},")
            out.append("CLOSE,")
    return "\n".join(out).rstrip(",") + "\n"


def icon_color(design):
    panels, (cx, cy, r) = glyph(design)
    s = 256 / 824
    ox = oy = -100 * s
    t = lambda p: (p[0] * s + ox, p[1] * s + oy)
    svg = read_source(design, 128)
    stops = re.findall(r'<linearGradient id="b\dr"[^>]*>(?:\s*<stop offset="[\d.]+" stop-color="#([0-9A-Fa-f]{6})"/>)+', svg)
    top = re.search(r'<linearGradient id="b\dr"[^>]*><stop offset="0" stop-color="#([0-9A-Fa-f]{6})"/><stop offset="1" stop-color="#([0-9A-Fa-f]{6})"/>', svg)
    assert top and stops, "card gradient not found"
    a, b = (int(top.group(1)[i:i + 2], 16) for i in (0, 2, 4)), (int(top.group(2)[i:i + 2], 16) for i in (0, 2, 4))
    mid = [(x + y) // 2 for x, y in zip(a, b)]
    gold = re.search(r'<stop offset="0.5" stop-color="#([0-9A-Fa-f]{6})"/>', svg).group(1)
    g = [int(gold[i:i + 2], 16) for i in (0, 2, 4)]
    hexs = lambda v: ", ".join(f"0x{c:02X}" for c in v)
    out = [ICON_HEAD + "CANVAS_DIMENSIONS, 256,", "NEW_PATH,", f"PATH_COLOR_ARGB, 0xFF, {hexs(mid)},",
           f"ROUND_RECT, 0, 0, 256, 256, {round(236 * s)},"]
    for poly, alpha in panels:
        q = [t(p) for p in poly]
        out += ["NEW_PATH,", "FILL_RULE_NONZERO,", f"PATH_COLOR_ARGB, 0x{round(alpha * 255):02X}, 0xFF, 0xFF, 0xFF,",
                f"MOVE_TO, {f32(q[0][0])}, {f32(q[0][1])},"]
        out += [f"LINE_TO, {f32(x)}, {f32(y)}," for x, y in q[1:]]
        out.append("CLOSE,")
    c = t((cx, cy))
    out += ["NEW_PATH,", f"PATH_COLOR_ARGB, 0xFF, {hexs(g)},", f"CIRCLE, {f32(c[0])}, {f32(c[1])}, {f32(r * s)}"]
    return "\n".join(out) + "\n"


# --- the slot -------------------------------------------------------------------------------------------

def slot(design):
    """name -> producer. Producers return bytes (text files) or write a PNG path."""
    files = {}
    for px in PNG_SIZES:
        files[f"generated/product_icon/{px}x{px}.png"] = ("svg", read_source(design, class_for(px)), px, px)
    files["branding/app_icon/raw.png"] = ("svg", read_source(design, 512), 1024, 1024)
    for name, w, h in (("product_logo.png", 140, 32), ("product_logo_white.png", 140, 32),
                       ("product_logo_200.png", 280, 64), ("product_logo_white_200.png", 280, 64)):
        files[f"branding/{name}"] = ("svg", wide(design, h, w), w, h)
    files["branding/product_logo_22_mono.png"] = ("svg", mono_svg(design, 22), 22, 22)
    for name, px in FAVICONS_BRANDED.items():
        files[f"favicons/{name}"] = ("svg", NTP_GLYPH.read_text(encoding="utf-8").replace("currentColor", "#000000"), px, px)
    files["branding/product_logo.svg"] = ("text", read_source(design, 512))
    files["branding/product_logo.icon"] = ("text", icon_mono(design))
    files["branding/product_logo_color.icon"] = ("text", icon_color(design))
    return files


def write(design):
    if not shutil.which("rsvg-convert"):
        sys.exit("rsvg-convert is needed to render the slot")
    for name, spec in slot(design).items():
        out = HERE / name
        out.parent.mkdir(parents=True, exist_ok=True)
        if spec[0] == "text":
            out.write_text(spec[1], encoding="utf-8")
        else:
            rsvg(spec[1], spec[2], spec[3], out)
    ver = subprocess.run(["rsvg-convert", "--version"], capture_output=True, text=True, check=True).stdout.strip()
    VERSION_FILE.write_text(ver + "\n", encoding="utf-8")
    write_manifest()
    print("slot written:", len(list(manifest_files())), "files")


def manifest_files():
    """Every file of the slot that regenerate.py makes, plus the files copied unchanged (favicons)."""
    for sub in ("branding", "favicons", "generated"):
        for p in sorted((HERE / sub).rglob("*")):
            if p.is_file():
                yield p


def write_manifest():
    lines = [f"{sha256(p)}  {p.relative_to(HERE)}" for p in manifest_files()]
    MANIFEST.write_text("\n".join(lines) + "\n", encoding="utf-8")


def png_size(path):
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", data[16:24])


def check(design):
    rc = 0
    # 1. manifest
    recorded = {}
    for line in MANIFEST.read_text(encoding="utf-8").splitlines():
        h, name = line.split("  ", 1)
        recorded[name] = h
    on_disk = {str(p.relative_to(HERE)): p for p in manifest_files()}
    for name in sorted(set(recorded) | set(on_disk)):
        if name not in on_disk:
            print(f"FAIL {name}: in MANIFEST.sha256, not on disk"); rc = 1
        elif name not in recorded:
            print(f"FAIL {name}: on disk, not in MANIFEST.sha256"); rc = 1
        elif sha256(on_disk[name]) != recorded[name]:
            print(f"FAIL {name}: sha256 differs from MANIFEST.sha256"); rc = 1
    if rc == 0:
        print(f"ok   manifest: {len(recorded)} files match")
    # 2. the slot has every name of the spec (patches-0.md section G) with the right shape
    want_png = {f"generated/product_icon/{px}x{px}.png": (px, px) for px in PNG_SIZES}
    want_png.update({"branding/app_icon/raw.png": (1024, 1024), "branding/product_logo.png": (140, 32),
                     "branding/product_logo_white.png": (140, 32), "branding/product_logo_200.png": (280, 64),
                     "branding/product_logo_white_200.png": (280, 64), "branding/product_logo_22_mono.png": (22, 22)})
    for name, wh in want_png.items():
        if name not in on_disk:
            print(f"FAIL {name}: missing"); rc = 1
        elif png_size(on_disk[name]) != wh:
            print(f"FAIL {name}: size {png_size(on_disk[name])}, want {wh}"); rc = 1
    for name in ("branding/product_logo.svg", "branding/product_logo.icon", "branding/product_logo_color.icon"):
        if name not in on_disk:
            print(f"FAIL {name}: missing"); rc = 1
    for name in ("branding/product_logo.icon", "branding/product_logo_color.icon"):
        if name in on_disk:
            text = on_disk[name].read_text(encoding="utf-8")
            # the icon files become C++ (vector_icons.cc): 128f is not a literal, 128.5f and 128 are
            if re.search(r"(?<![\d.])\d+f\b", text) or not text.rstrip().endswith(("CLOSE", "f", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9")):
                print(f"FAIL {name}: a number is not a C++ literal or the file is cut"); rc = 1
    favs = [n for n in on_disk if n.startswith("favicons/")]
    if len(favs) != 20:
        print(f"FAIL favicons: {len(favs)} files, want 20"); rc = 1
    for name, px in FAVICONS_BRANDED.items():
        if png_size(HERE / "favicons" / name) != (px, px):
            print(f"FAIL favicons/{name}: wrong size"); rc = 1
    if rc == 0:
        print("ok   slot: names and sizes of patches-0.md section G")
    # 3. optional: render again and compare pixels
    if design:
        if not shutil.which("rsvg-convert"):
            print("WARN rsvg-convert not found: pixels not compared")
        else:
            tmp = HERE / ".render-check"
            tmp.mkdir(exist_ok=True)
            try:
                for name, spec in slot(design).items():
                    if spec[0] == "text":
                        if (HERE / name).read_text(encoding="utf-8") != spec[1]:
                            print(f"FAIL {name}: differs from a fresh render"); rc = 1
                        continue
                    out = tmp / "x.png"
                    rsvg(spec[1], spec[2], spec[3], out)
                    if not same_pixels(out, HERE / name):
                        print(f"FAIL {name}: pixels differ from a fresh render"); rc = 1
            finally:
                shutil.rmtree(tmp, ignore_errors=True)
            if rc == 0:
                print("ok   render: every file equals a fresh render of the design drawings")
    return rc


def same_pixels(a, b):
    try:
        from PIL import Image, ImageChops  # pylint: disable=import-outside-toplevel
    except ImportError:
        return sha256(a) == sha256(b)
    ia, ib = Image.open(a).convert("RGBA"), Image.open(b).convert("RGBA")
    if ia.size != ib.size:
        return False
    diff = ImageChops.difference(ia, ib).getextrema()
    return max(hi for _, hi in diff) <= 2


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--design", help="the design repository (default: $ATLAS_DESIGN or ../atlas-brand)")
    p.add_argument("--write", action="store_true")
    p.add_argument("--check", action="store_true")
    a = p.parse_args()
    design = a.design or os.environ.get("ATLAS_DESIGN") or (str(ROOT.parent / "atlas-brand") if (ROOT.parent / "atlas-brand").is_dir() else None)
    if a.write:
        if not design:
            sys.exit("--write needs --design DIR")
        write(design)
        return 0
    if a.check:
        return check(a.design or os.environ.get("ATLAS_DESIGN"))
    p.error("one of --write or --check")


if __name__ == "__main__":
    sys.exit(main())
