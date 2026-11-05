# SPDX-License-Identifier: GPL-3.0-or-later
"""A rewrite pass over the strings of the engine (.grd, .grdp and the .xtb translations).

The message ids of grit are fingerprints of the English text, so changing the text of a message
moves its id, and every translation of it must move with it or it is lost. This is the same method
as utils/name_substitution.py (the fork's own pass, which made the names; it uses its helpers for the
fingerprint), with two differences: the rules are ours, and a translation keeps its id in step even
when its own text has nothing to rewrite (a translator may have left the name out).

grit_pass(tree, rules, apply, prefix) -> dict of counts, rules = [(name, compiled regex, replacement)].
"""

import multiprocessing
import os
import sys
import xml.etree.ElementTree as xml
from pathlib import Path

from _lib import SKIP_DIRS, read_text, write_text

# the engine directories whose strings are not in the browser we build (the fork's pass skips them too)
IGNORE_DIRS = {"chromeos", "remoting", "ash", "testdata"} | SKIP_DIRS

_util = None


def _fork_util(tree):
    """The fork's name_substitution_utils (fingerprints, parser), imported from the fork's utils/."""
    global _util
    if _util is None:
        root = Path(__file__).resolve().parent.parent.parent
        sys.path.insert(0, str(root / "utils"))
        import name_substitution_utils as util  # pylint: disable=import-outside-toplevel

        util.add_grit_to_path(Path(tree))
        _util = util
    return _util


def _replace_text(text, rules, hits):
    if not text:
        return text
    for name, regex, repl in rules:
        text, n = regex.subn(repl, text)
        if n:
            hits[name] = hits.get(name, 0) + n
    return text


def _replace_node(node, rules, hits):
    """Rewrite text and tails of a <message>/<translation> and everything inside it."""
    before = sum(hits.values())
    node.text = _replace_text(node.text, rules, hits)
    node.tail = _replace_text(node.tail, rules, hits)
    for child in node:
        _replace_node(child, rules, hits)
    return sum(hits.values()) != before


def _serialize(root, original):
    """The file as xml text, with an <?xml?> declaration only if the original had one: some readers (the
    inherited setup page's string generator) take the first node as the root and break on a new declaration."""
    declaration = original.lstrip().startswith("<?xml")
    out = xml.tostring(root, encoding="unicode", xml_declaration=declaration)
    return out.replace("!!dollar-sign-literal!!", "&#36;")


def _grit_file(args):
    path, tree, rules, apply = args
    util = _fork_util(tree)
    original = read_text(path)
    text = original.replace("&#36;", "!!dollar-sign-literal!!")
    root = xml.fromstring(text, util.get_parser())
    fp_map, hits, messages = {}, {}, 0
    for message in root.findall(".//message"):
        old_fp = util.compute_fp(message)
        if _replace_node(message, rules, hits):
            messages += 1
            new_fp = util.compute_fp(message)
            if old_fp != new_fp:
                fp_map[old_fp] = new_fp
    if not messages:
        return None
    if apply:
        write_text(path, _serialize(root, original))
    return str(path), messages, hits, fp_map


def _xtb_file(args):
    path, tree, rules, fp_map, apply = args
    util = _fork_util(tree)
    original = read_text(path)
    text = original.replace("&#36;", "!!dollar-sign-literal!!")
    root = xml.fromstring(text, util.get_parser())
    hits, changed, translations = {}, False, 0
    seen = set()
    for tr in root.findall(".//translation"):
        old_id = tr.get("id")
        assert old_id, f"{path}: translation without id"
        if _replace_node(tr, rules, hits):
            changed = True
            translations += 1
        if old_id in fp_map:
            tr.set("id", fp_map[old_id])
        util.dedup_translations_in_place(tr, seen)
        if tr.get("id") != old_id:
            changed = True
    if not changed:
        return None
    if apply:
        write_text(path, _serialize(root, original))
    return str(path), translations, hits


def _candidates(tree, exts):
    tree = Path(tree)
    for root, dirs, files in os.walk(tree):
        dirs[:] = [d for d in dirs if d not in IGNORE_DIRS]
        for name in files:
            if not name.startswith(".") and name.rsplit(".", 1)[-1].lower() in exts:
                yield Path(root) / name


def grit_pass(tree, rules, apply, prefix):
    """Run RULES over the grd/grdp of TREE, then over the xtb with the ids that moved.

    Returns {prefix.grit.files, prefix.grit.messages, prefix.xtb.files, prefix.xtb.translations,
    prefix.grit.rule.<rule>, prefix.xtb.rule.<rule>: matches}."""
    tree = Path(tree)
    _fork_util(tree)
    ctx = multiprocessing.get_context("fork")
    counts = {f"{prefix}.grit.files": 0, f"{prefix}.grit.messages": 0,
              f"{prefix}.xtb.files": 0, f"{prefix}.xtb.translations": 0}
    for name, _, _ in rules:
        counts[f"{prefix}.grit.rule.{name}"] = 0
    workers = min(os.cpu_count() or 1, 32)
    with ctx.Pool(workers) as pool:
        fp_map = {}
        grit_args = [(p, tree, rules, apply) for p in _candidates(tree, {"grd", "grdp"})]
        for res in pool.imap_unordered(_grit_file, grit_args, chunksize=4):
            if res is None:
                continue
            _, messages, hits, fps = res
            counts[f"{prefix}.grit.files"] += 1
            counts[f"{prefix}.grit.messages"] += messages
            for name, n in hits.items():
                counts[f"{prefix}.grit.rule.{name}"] += n
            fp_map.update(fps)
        xtb_args = [(p, tree, rules, fp_map, apply) for p in _candidates(tree, {"xtb"})]
        for res in pool.imap_unordered(_xtb_file, xtb_args, chunksize=8):
            if res is None:
                continue
            _, translations, hits = res
            counts[f"{prefix}.xtb.files"] += 1
            counts[f"{prefix}.xtb.translations"] += translations
            for name, n in hits.items():
                key = f"{prefix}.xtb.rule.{name}"
                counts[key] = counts.get(key, 0) + n
    return counts
