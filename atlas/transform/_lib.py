# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared helpers of the Atlas transforms (atlas/transform/*.py).

A transform is a scripted rewrite of the prepared source tree with ASSERTED MATCH COUNTS:

  * a rule that matches nothing is an error (the engine moved under us, or the rule is stale);
  * a rule whose count differs from the one recorded in atlas/tests/transform-counts.json is a
    warning with the old and new number (the file lists what changed; run with --update-counts after
    a human looked at the diff).

Every transform has the same command line:

  TRANSFORM.py --check TREE      count the matches on a tree that has not been transformed, write nothing
  TRANSFORM.py --apply TREE      rewrite TREE (once; a second run finds nothing and fails on purpose)
  TRANSFORM.py --verify TREE     the tree is transformed: no match of any rule is left
  ... --strict                   a changed count is an error too
  ... --update-counts            write the measured counts into transform-counts.json (--check only)
"""

import argparse
import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
COUNTS = ROOT / "atlas" / "tests" / "transform-counts.json"

# directories that are never part of the shipped browser or never ours to rewrite
SKIP_DIRS = {"out", ".git", ".pc", "node_modules"}


def load_counts():
    with open(COUNTS, encoding="utf-8") as f:
        return json.load(f)


def save_counts(data):
    with open(COUNTS, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True, ensure_ascii=False)
        f.write("\n")


def need_tree(tree):
    tree = Path(tree)
    if not (tree / "OWNERS").is_file() or not (tree / "chrome").is_dir():
        sys.exit(f"{tree}: not an engine source tree (no OWNERS / chrome)")
    return tree


def read_text(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


def write_text(path, text):
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def walk(tree, exts, skip_parts=()):
    """Yield files of TREE with one of EXTS (lower case, with the dot), skipping SKIP_DIRS."""
    tree = Path(tree)
    for root, dirs, files in os.walk(tree):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS and d not in skip_parts]
        for name in files:
            if name.startswith("."):
                continue
            if os.path.splitext(name)[1].lower() in exts:
                yield Path(root) / name


class Result:
    """Counts of one run: key -> number of matches."""

    def __init__(self):
        self.counts = {}
        self.notes = []

    def add(self, key, n=1):
        self.counts[key] = self.counts.get(key, 0) + n

    def declare(self, key):
        self.counts.setdefault(key, 0)


def judge(result, prefix, args):
    """Compare RESULT with the recorded counts. Returns the process exit code."""
    recorded = load_counts().get("expected", {})
    rc = 0
    for key in sorted(result.counts):
        got = result.counts[key]
        want = recorded.get(key)
        if got == 0:
            print(f"FAIL {key}: no match (the rule is stale or the engine moved)")
            rc = 1
        elif want is None:
            print(f"WARN {key}: {got} matches, no recorded count")
            if args.strict:
                rc = 1
        elif want != got:
            print(f"WARN {key}: {got} matches, recorded {want} (changed by {got - want:+d})")
            if args.strict:
                rc = 1
        else:
            print(f"ok   {key}: {got}")
    stale = sorted(k for k in recorded if k.startswith(prefix) and k not in result.counts)
    for key in stale:
        print(f"WARN {key}: recorded but not produced by this run")
        if args.strict:
            rc = 1
    if args.update_counts:
        data = load_counts()
        exp = data.setdefault("expected", {})
        for key in list(exp):
            if key.startswith(prefix):
                del exp[key]
        exp.update({k: v for k, v in result.counts.items() if v})
        save_counts(data)
        print(f"counts of {prefix if isinstance(prefix, str) else ', '.join(prefix)} written to {COUNTS.relative_to(ROOT)}")
    return rc


def parser(doc):
    p = argparse.ArgumentParser(description=doc)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--check", metavar="TREE", help="count matches, write nothing")
    g.add_argument("--apply", metavar="TREE", help="rewrite the tree")
    g.add_argument("--verify", metavar="TREE", help="no match is left on a transformed tree")
    p.add_argument("--strict", action="store_true", help="a changed count is an error")
    p.add_argument("--update-counts", action="store_true", help="record the measured counts (with --check)")
    return p


def mode_of(args):
    if args.check:
        return "check", need_tree(args.check)
    if args.apply:
        return "apply", need_tree(args.apply)
    return "verify", need_tree(args.verify)


def verify_report(left, what):
    """LEFT: list of 'path:line text'. Prints and returns the exit code."""
    if not left:
        print(f"ok   {what}: nothing left")
        return 0
    for item in left[:40]:
        print(f"FAIL {what}: {item}")
    if len(left) > 40:
        print(f"FAIL {what}: ... and {len(left) - 40} more")
    return 1


def text_rules(tree, rules, apply, result, missing_ok=False):
    """RULES: [(key, relative path, compiled regex, replacement)]. One key may name several files
    (the counts add up). Counts go to RESULT; with APPLY the files are rewritten."""
    tree = Path(tree)
    by_file = {}
    for key, rel, regex, repl in rules:
        result.declare(key)
        by_file.setdefault(rel, []).append((key, regex, repl))
    for rel, items in by_file.items():
        path = tree / rel
        if not path.is_file():
            if not missing_ok:
                result.notes.append(f"missing file {rel}")
            continue
        text = read_text(path)
        new = text
        for key, regex, repl in items:
            new, n = regex.subn(repl, new)
            result.add(key, n)
        if apply and new != text:
            write_text(path, new)


def finish(result, prefixes, args, mode):
    """Print the verdict of a run and return the exit code. In verify mode every count must be 0."""
    prefixes = tuple(prefixes)
    for note in result.notes:
        print(f"FAIL {note}")
    rc = 1 if result.notes else 0
    if mode == "verify":
        for key in sorted(result.counts):
            if result.counts[key]:
                print(f"FAIL {key}: {result.counts[key]} left")
                rc = 1
        if rc == 0:
            print(f"ok   {', '.join(prefixes)}: nothing left to rewrite")
        return rc
    return judge(result, prefixes, args) or rc
