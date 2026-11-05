#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""kit_metrics.py: holds the numbers of the Kit chrome's dialogs, sheets, tooltips and menus
(chrome/browser/ui/atlas/kit/kit_metrics.h) to the Design System specs.

  python3 -I atlas/transform/kit_metrics.py --check DESIGN_REPO     fail on drift
  python3 -I atlas/transform/kit_metrics.py --list                  print the table

DESIGN_REPO is the design repo with components/*.toml (branch wave3/ds-components or later). Every constant of
kit_metrics.h appears once in TABLE below with the spec file and the place in it; a number that moved in a spec, a
spec that lost its sentence, a constant not in the table and a table row without a constant all fail. A spec file
missing from DESIGN_REPO fails too, except those in OPTIONAL (button.toml lives on a design branch that may not be
merged yet): they are reported as skipped.
"""
import os
import re
import sys
import tomllib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
HEADER = os.path.join(ROOT, "chrome", "browser", "ui", "atlas", "kit", "kit_metrics.h")
OPTIONAL = {"button.toml"}

# (constant, spec file, path of the value in the TOML, regex with one group over a string, or None for a number)
TABLE = [
    ("kSheetRadius", "sheet.toml", "anatomy.surface", r"radius (\d+)"),
    ("kSheetInset", "sheet.toml", "anatomy.surface", r"(\d+) px inside"),
    ("kSheetButtonGap", "sheet.toml", "anatomy.buttons", r"(\d+) px apart"),
    ("kSideSheetWidth", "side-sheet.toml", "sizes.pane.width", None),
    ("kSideSheetInset", "side-sheet.toml", "anatomy.pane", r"the (\d+) px inset"),
    ("kBottomSheetRadius", "bottom-sheet.toml", "anatomy.surface", r"radius (\d+) at the top"),
    ("kBottomSheetHandleWidth", "bottom-sheet.toml", "sizes.handle.width", None),
    ("kBottomSheetHandleHeight", "bottom-sheet.toml", "sizes.handle.height", None),
    ("kBottomSheetInset", "bottom-sheet.toml", "anatomy.content", r"the (\d+) px inset"),
    ("kRichTooltipRadius", "rich-tooltip.toml", "anatomy.surface", r"radius (\d+)"),
    ("kRichTooltipInset", "rich-tooltip.toml", "anatomy.surface", r"the (\d+) px inset"),
    ("kRichTooltipLineChars", "rich-tooltip.toml", "anatomy.text", r"at most (\d+) characters"),
    ("kMenuRadius", "menu.toml", "anatomy.surface", r"radius (\d+)"),
    ("kMenuItemCompact", "menu.toml", "density.compact.item", None),
    ("kMenuItemStandard", "menu.toml", "density.standard.item", None),
    ("kMenuItemComfortable", "menu.toml", "density.comfortable.item", None),
    ("kButtonRadius", "button.toml", "anatomy.shape", r"radius (\d+)"),
    ("kButtonPadding", "button.toml", "anatomy.shape", r"padding 0 (\d+)"),
]


def header_constants(path=HEADER):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    return {m.group(1): int(m.group(2)) for m in re.finditer(r"inline constexpr int (k\w+) = (\d+);", text)}


def lookup(doc, dotted):
    node = doc
    for part in dotted.split("."):
        if not isinstance(node, dict) or part not in node:
            return None
        node = node[part]
    return node


def check(design, header=HEADER):
    consts = header_constants(header)
    problems, skipped, seen = [], [], set()
    for name, fname, where, pattern in TABLE:
        seen.add(name)
        if name not in consts:
            problems.append("%s is in the table but not in kit_metrics.h" % name)
            continue
        path = os.path.join(design, "components", fname)
        if not os.path.isfile(path):
            (skipped if fname in OPTIONAL else problems).append("%s: %s not in %s" % (name, fname, design))
            continue
        with open(path, "rb") as f:
            doc = tomllib.load(f)
        value = lookup(doc, where)
        if value is None:
            problems.append("%s: %s has no %s" % (name, fname, where))
            continue
        if pattern is not None:
            m = re.search(pattern, str(value))
            if not m:
                problems.append("%s: %s %s no longer says /%s/ (%r)" % (name, fname, where, pattern, value))
                continue
            value = int(m.group(1))
        if value != consts[name]:
            problems.append("%s: kit_metrics.h says %d, %s %s says %s" % (name, consts[name], fname, where, value))
    for name in sorted(set(consts) - seen):
        problems.append("%s is in kit_metrics.h but not in the table of kit_metrics.py" % name)
    return problems, skipped


def main(argv):
    if argv[1:2] == ["--list"]:
        consts = header_constants()
        for name, fname, where, _ in TABLE:
            print("%-26s %-4s %s %s" % (name, consts.get(name, "?"), fname, where))
        return 0
    if len(argv) == 3 and argv[1] == "--check":
        problems, skipped = check(argv[2])
        for s in skipped:
            print("skip " + s)
        for p in problems:
            print("FAIL " + p)
        if not problems:
            print("ok   %d numbers of kit_metrics.h equal the specs (%d skipped)" % (len(TABLE) - len(skipped), len(skipped)))
        return 1 if problems else 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
