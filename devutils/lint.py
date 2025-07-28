#!/usr/bin/env python3

# Copyright 2025 The Helium Authors
# You can use, redistribute, and/or modify this source code under
# the terms of the GPL-3.0 license that can be found in the LICENSE file.

import sys
import inspect
import argparse
import _lint_tests
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('-t', '--tree', help='root of the source tree to check')
    return parser.parse_args()


def main():
    args = parse_args()
    root_dir = (Path(__file__).parent / "..").resolve()

    if args.tree:
        root_dir = Path(args.tree).resolve()

    _lint_tests._init(root_dir)

    for name, fn in inspect.getmembers(_lint_tests, inspect.isfunction):
        if name.startswith("_"):
            continue

        try:
            fn(root_dir)
            print(f"[OK] {name}")
        except Exception as e:
            print(f"[ERR] {name}:", file=sys.stderr)
            raise


if __name__ == '__main__':
    main()
