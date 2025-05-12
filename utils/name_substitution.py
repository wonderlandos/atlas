#!/usr/bin/env python3
# Copyright 2025 The Helium Authors
# You can use, redistribute, and/or modify this source code under
# the terms of the GPL-3.0 license that can be found in the LICENSE file.

from pathlib import Path
import argparse
import tarfile
import asyncio
import os
import re

REPLACEMENT_REGEXES_STR = [
    # stuff we don't want to replace
    (r'(\w+) Root Program',   r'\1_unreplace Root Program'),
    (r'(\w+) Web( S|s)tore',  r'\1_unreplace Web Store'),
    (r'(\w+) Remote Desktop', r'\1_unreplace Remote Desktop'),
    (r'("BEGIN_LINK_CHROMIUM")(.*?Chromium)(.*?<ph name="END_LINK_CHROMIUM")', r'\1\2_unreplace\3'),

    # main replacement(s)
    (r'(?:Google )?Chrom(e|ium)(?!\w)', r'Helium'),

    # post-replacement cleanup
    (r'((?:Google )?Chrom(e|ium))_unreplace', r'\1'),
    (r'_unreplace', r'')
]

REPLACEMENT_REGEXES = list(
    map(
        lambda line: (re.compile(line[0]), line[1]),
        REPLACEMENT_REGEXES_STR
    )
)

def replace(str):
    for regex, replacement in REPLACEMENT_REGEXES:
        str = re.sub(regex, replacement, str)
    return str

def replacement_sanity():
    before_after = [
        ('Chrome Root Program', 'Chrome Root Program'),
        ('Chrome Web Store', 'Chrome Web Store'),
        ('Chromium Web Store', 'Chromium Web Store'),
        ('Chrome Remote Desktop', 'Chrome Remote Desktop'),
        ('Google Chrome', 'Helium'),
        ('Chrome Google Chrome Chrome Chromium', 'Helium Helium Helium Helium'),
        ('Chrome', 'Helium'),
        ('Chromium', 'Helium'),
    ]

    for a, b in before_after:
        assert(replace(a) == b)

def parse_args():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--sub', action='store_true')
    group.add_argument('--unsub', action='store_true')
    parser.add_argument('--backup-path', metavar='<tarball-path>', type=Path)
    parser.add_argument('-t', metavar='source_tree', type=Path, required=True)

    args = parser.parse_args()

    if args.unsub and not args.backup_path:
        raise Exception("backup_path is missing, but unsub was specified")

    return args

def get_substitutable_files(tree):
    out = tree / 'out'

    for root, _, files in os.walk(tree):
        root = Path(root)
        if out in root.parents:
            continue

        yield from map(
            lambda filename: root / filename,
            filter(
                lambda filename: not filename.startswith('.') and (
                    filename.endswith('.xtb') or filename.endswith('.grd') or filename.endswith('.grdp')
                ),
                files
            )
        )

async def substitute_file(tree, path, tarball = None):
    arcname = str(path.relative_to(tree))
    text = None

    with open(path, 'r') as f:
        text = f.read()

    replaced = replace(text)
    if text != replaced:
        print(f"Replaced strings in {arcname}")
        if tarball:
            tarball.add(path, arcname=arcname, recursive=False)
        with open(path, 'w') as f:
            f.write(replaced)

def do_unsubstitution(tree, tarpath):
    with tarfile.open(str(tarpath), 'r:gz') as tar:
        tar.extractall(path=tree, filter='fully_trusted')
    tarpath.unlink()

async def do_substitution(tree, tarpath):
    with tarfile.open(str(tarpath), 'w:gz') if tarpath else open(os.devnull, 'w') as cache_tar:
        pending_substitutions = map(
            lambda path: substitute_file(tree, path, cache_tar if tarpath else None),
            get_substitutable_files(tree)
        )
        await asyncio.gather(*pending_substitutions)

async def main():
    replacement_sanity()
    args = parse_args()

    if not (args.t / "OWNERS").exists():
        raise Exception("wrong src directory")

    if args.sub:
        if args.backup_path is not None and args.backup_path.exists():
            raise Exception("unsub tarball already exists, aborting")
        await do_substitution(args.t, args.backup_path)
    elif args.unsub and args.backup_path:
        do_unsubstitution(args.t, args.backup_path)


if __name__ == '__main__':
    asyncio.run(main())
