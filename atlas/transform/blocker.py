#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""T-07 blocker licences: what the package may not carry leaves it.

The bundled blocker lists a server list under a noncommercial licence (Peter Lowe's list, entry
"plowe-0" of third_party/ublock/assets/assets.json, files under assets/thirdparties/pgl.yoyo.org).
It is on by default in the blocker's own settings. Atlas is distributed under licences that allow
commercial use, so the entry and its files are removed here; the other lists stay.

See _lib.py for the command line and for what the counts mean.
"""

import re
import shutil
import sys

sys.dont_write_bytecode = True
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import _lib  # noqa: E402

ASSETS = "third_party/ublock/assets/assets.json"
LISTS = "third_party/ublock/assets/thirdparties/pgl.yoyo.org"
ENTRY = re.compile(r'\t"plowe-0": \{\n(?:\t\t.*\n|\t\t\t.*\n)*?\t\},\n')


def run(tree, apply):
    result = _lib.Result()
    tree = Path(tree)
    result.declare("blocker.plowe_entry")
    result.declare("blocker.plowe_files")
    path = tree / ASSETS
    if path.is_file():
        text = _lib.read_text(path)
        new, n = ENTRY.subn("", text)
        result.add("blocker.plowe_entry", n)
        if apply and n:
            _lib.write_text(path, new)
    else:
        result.notes.append(f"missing file {ASSETS}")
    lists = tree / LISTS
    if lists.is_dir():
        result.add("blocker.plowe_files", sum(1 for p in lists.rglob("*") if p.is_file()))
        if apply:
            shutil.rmtree(lists)
    return result


def main():
    args = _lib.parser(__doc__.split("\n\n")[0]).parse_args()
    mode, tree = _lib.mode_of(args)
    result = run(tree, apply=(mode == "apply"))
    return _lib.finish(result, ["blocker."], args, mode)


if __name__ == "__main__":
    sys.exit(main())
