#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""i18n_atlas.py -t TREE [--source FILE] [--translations DIR]: the i18n step for the Atlas strings.

utils/i18n_apply.py (the inherited step, run first) looks for the .grd that includes a .grdp in the directory of
that .grdp. Our parts live in chrome/app/atlas/ but are included by chrome/app/generated_resources.grd (patch
0010), so it cannot place them. This script reads atlas/i18n/source.atlas.json and atlas/i18n/translations/<lang>.json
(the same format as i18n/), finds the XTB file of each language in generated_resources.grd and inserts the
translations with the functions of utils/i18n_apply.py (the same message fingerprint, the same XTB insert).
A language of i18n/languages.json without a file here is skipped (English is the fallback); a file whose
translations land nowhere is an error. Python standard library plus the engine's grit (tree/tools/grit)."""
import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "utils"))
PARENT_GRD = "chrome/app/generated_resources.grd"


def run(tree, source_path, trans_dir, languages_path, out=print):
    import i18n_apply as base            # the inherited functions: fingerprint, XTB insert
    import name_substitution_utils as namesub
    namesub.add_grit_to_path(tree)
    source = json.loads(Path(source_path).read_text(encoding="utf-8"))
    languages = json.loads(Path(languages_path).read_text(encoding="utf-8"))
    grd = tree / PARENT_GRD
    xtbs = base.parse_xtb_paths(grd)
    result = {}
    for code in languages:
        tpath = Path(trans_dir) / f"{code}.json"
        if not tpath.is_file():
            continue
        translations = json.loads(tpath.read_text(encoding="utf-8"))
        by_key = {(t["name"], t["source"]): t for t in translations if t}
        xtb_lang = "no" if code == "nb" else code
        xtb = xtbs.get(xtb_lang)
        if xtb is None or not xtb.is_file():
            raise SystemExit(f"i18n_atlas: no XTB for {code} in {PARENT_GRD}")
        n = base.merge_into_xtb(xtb, source, by_key)
        if n == 0:
            raise SystemExit(f"i18n_atlas: {code}: no translation of {tpath.name} matched a source message")
        result[code] = n
        out(f"{code}: applied {n} Atlas translations to {xtb.name}")
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("-t", "--tree", type=Path, required=True)
    ap.add_argument("--source", type=Path, default=ROOT / "atlas/i18n/source.atlas.json")
    ap.add_argument("--translations", type=Path, default=ROOT / "atlas/i18n/translations")
    ap.add_argument("--languages", type=Path, default=ROOT / "i18n/languages.json")
    a = ap.parse_args()
    res = run(a.tree, a.source, a.translations, a.languages)
    if not res:
        raise SystemExit("i18n_atlas: nothing applied")


if __name__ == "__main__":
    sys.exit(main())
