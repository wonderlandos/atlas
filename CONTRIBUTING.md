# Contributing to Atlas

Atlas is the WolOS browser. This repository is its patch set: the inherited patch series, mirrored
from upstream on the branch `upstream`, plus the Atlas overlay in `atlas/` on the branch `main`.
Read [atlas/README.md](atlas/README.md) for the model; this page is the working rules.

## Conduct

Every WolOS space, this repository included, follows
[The Wonderland Doctrine](https://github.com/wonderlandos/wolos/blob/main/docs/DOCTRINE.md):
be blunt about the work, never about the person.

## Where things go

| You want to | Do this |
|---|---|
| change a default, remove a service, change behaviour | an overlay patch in `atlas/patches/<area>/` (below) |
| change a build flag | a line in `atlas/flags.atlas.gn`, with its measured effect in `atlas/debloat.toml` when it removes code |
| rename or substitute strings across the tree | a scripted transform in `atlas/transform/` with asserted match counts, not a patch |
| change a policy or a first-run preference | the WolOS system package (repository `wonderlandos/wolos`), not this repository |
| change the Linux build or packaging | `atlas/platform/` |
| drop an inherited patch | a line in `atlas/drop.list` (leaf patches only; `atlas/tools/atlas-series list` checks it) |
| fix an inherited patch | an overlay patch after it; never edit a file under `patches/` |

Pick the cheapest mechanism that works, in this order: GN argument, policy or first-run
preference, launcher flag, scripted transform, a small patch, a new component. The overlay budget
is **60 patches** and 2,500 added C++ lines. A patch that fails to apply for two engine milestones
in a row is deleted or rewritten, not carried by force.

## Adding a patch

1. Prepare a base tree (a prepared engine source tree with the inherited and platform series
   applied, see [atlas/README.md](atlas/README.md)) and apply the current overlay with
   `tools/apply-overlay.sh TREE`.
2. Make the change in the tree. Write it as `atlas/patches/<area>/NNNN-name.patch` with
   `diff -u` (labels `a/<path>` and `b/<path>`). `NNNN` is the next free number across all areas;
   numbers are never reused.
3. Start the file with the patch header (Why, Files, Test filter, Rebase risk, Release, License,
   Verified at; the format is in [atlas/README.md](atlas/README.md)). "Why" says what changes for
   the person using Atlas and why a cheaper mechanism was not enough.
4. Append the path to `atlas/patches/series`.
5. Check on a fresh base tree: `tools/apply-overlay.sh --dry-run TREE` prints `ok` for every patch.
6. Commit the patch file and the series line together, **one commit per patch**, with the header as
   the commit message (first line: `Atlas overlay patch NNNN: <area>/<name>`).

To change an existing patch, amend its own commit (rebase `main`), so the history keeps one commit
per patch.

## Syncing with upstream

`tools/sync-upstream.sh` does the whole cycle and never pushes:

1. fetches the upstream remote with tags and picks its newest release tag;
2. fast-forwards `upstream` to it (the mirror is never edited or rewritten);
3. replays the overlay commits one by one onto the release and reports each as `ok` or
   `CONFLICT` with the files and the patch;
4. with `--tree DIR` (a base tree of the new release) also dry-runs the overlay against the new
   engine code, per patch;
5. moves `main` only when everything replayed cleanly.

A conflict is always fixed in our patch, never in an inherited one: rebase `main` onto the tag,
fix the patch, regenerate it from a base tree, keep it in its own commit. The lead pushes
`upstream` and `main` (`main` with `--force-with-lease`, since it is rebased).

## Building

Atlas is built on the WolOS build forge (`atlas/platform/scripts/build.sh`, then
`atlas/platform/scripts/package.sh`); there is no hosted build. The repository CI only lints:
shell scripts, the series files and the patch headers, and the inherited series checks.

## Git style

- One commit per change: a patch, a flag, a tool, a document. No merge commits on `main`.
- Subject: `<area>: <what changes>` in plain words, or the patch header's first line for patches.
- Body: why, in full sentences; mention what is not done.

## Naming

The browser, its pages, its package, its patches and its documentation say **Atlas**. The project
Atlas is built on is credited by name in [README.md](README.md) and in the licence files, and
nowhere else; code identifiers inherited from it (symbols, file names of inherited tools) stay as
they are.

## Licensing

Overlay patches that modify files the inherited series patches, and changes to inherited scripts,
are GPL-3.0-or-later. New source files added to the engine tree are BSD-3-Clause. Inherited files
keep their licences ([LICENSE](LICENSE), [LICENSE.ungoogled_chromium](LICENSE.ungoogled_chromium)).
