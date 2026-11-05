#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""All Atlas transforms in the order the build needs them, then the brand slot.

  apply.py --check TREE    count the matches of every transform on an untransformed tree (writes nothing)
  apply.py --apply TREE    brand strings, the atlas:// scheme, the hosts, the product words, the blocker lists whose licence does not fit, then copy the brand slot
                           (atlas/resources/atlas_resources.txt) over the tree; writes TREE/.atlas.stamp
  apply.py --verify TREE   the tree is transformed: nothing left to rewrite, every slot file in place

The build runs `--apply` once, after the stage that makes the inherited names and pictures
(`resources`), as the stage `atlas`. The copy of the slot is the last thing done, so no picture of
the old brand survives. A second `--apply` fails on purpose (zero matches).
"""

import hashlib
import sys

sys.dont_write_bytecode = True  # the tools are run with -I from a checkout: leave no __pycache__
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import _lib  # noqa: E402
import blocker  # noqa: E402
import brand  # noqa: E402
import hosts  # noqa: E402
import scheme  # noqa: E402
import words  # noqa: E402

ROOT = HERE.parent.parent
SLOT = ROOT / "atlas" / "resources"


def slot_pairs():
    """(source file, engine path) for every line of atlas_resources.txt."""
    pairs = []
    for line in (SLOT / "atlas_resources.txt").read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            src, dst = line.split()
            pairs.append((SLOT / src, dst))
    return pairs


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def copy_slot(tree, apply, result):
    """Copy the slot. In check mode: every destination must exist (the engine has the place)."""
    import shutil  # pylint: disable=import-outside-toplevel

    for src, dst in slot_pairs():
        if not src.is_file():
            result.notes.append(f"slot file missing: {src.relative_to(ROOT)}")
            continue
        out = Path(tree) / dst
        if not out.parent.is_dir():
            result.notes.append(f"no directory for {dst}")
            continue
        result.add("resources.files")
        if apply:
            shutil.copyfile(src, out)


def verify_slot(tree):
    rc = 0
    for src, dst in slot_pairs():
        out = Path(tree) / dst
        if not out.is_file() or sha(out) != sha(src):
            print(f"FAIL {dst}: not the Atlas file {src.relative_to(ROOT)}")
            rc = 1
    if rc == 0:
        print(f"ok   resources: {len(slot_pairs())} files equal the brand slot")
    return rc


def main():
    args = _lib.parser(__doc__.split("\n\n")[0]).parse_args()
    mode, tree = _lib.mode_of(args)
    rc = 0
    for name, mod in (("brand", brand), ("scheme", scheme), ("hosts", hosts), ("words", words), ("blocker", blocker)):
        result = mod.run(tree, apply=(mode == "apply"))
        rc |= _lib.finish(result, [name + "."], args, mode)
        if rc and mode == "apply":
            print(f"apply.py: {name} failed, stopping (the tree is half transformed: start from a fresh one)")
            return rc
    if mode == "verify":
        rc |= verify_slot(tree)
    else:
        result = _lib.Result()
        copy_slot(tree, mode == "apply", result)
        rc |= _lib.finish(result, ["resources."], args, "check")
    if mode == "apply" and rc == 0:
        (tree / ".atlas.stamp").write_text("atlas transforms applied\n", encoding="utf-8")
    return rc


if __name__ == "__main__":
    sys.exit(main())
