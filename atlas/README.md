# atlas/: the overlay

Atlas is built from two layers kept apart on purpose:

1. **The upstream mirror.** The branch `upstream` is an exact copy of the upstream repository at a
   release tag. It is only ever fast-forwarded (`tools/sync-upstream.sh`), never edited. Its patch
   series (`patches/series`, the "inherited series") is applied as it is.
2. **The overlay.** The branch `main` is `upstream` plus a short, straight line of our own commits,
   **one commit per change**: one commit per overlay patch, per flag change, per tool. Everything
   that changes the browser lives in this directory; outside it, `main` only differs from `upstream`
   in the repository's own paperwork (README, CONTRIBUTING, AGENTS, `.github/`) and in `tools/`.

On every upstream release we fast-forward `upstream` and rebase the overlay commits onto it
(`tools/sync-upstream.sh` replays them one by one and reports conflicts per commit and per patch).
Because the inherited patch files are never edited, a conflict can only come from our own patches
meeting new engine code, and it is always fixed in our patch.

## Layout

| Path | What |
|---|---|
| `patches/series` | the overlay series, applied after the inherited series and the platform series |
| `patches/<area>/NNNN-name.patch` | one overlay patch; `NNNN` is unique across areas and never reused; areas: `privacy`, `security`, `brand`, `memory`, `host`, `ui`, `debloat` |
| `flags.atlas.gn` | our GN arguments, appended after `flags.gn` and `platform/flags.atlas.linux.gn` (the last assignment wins) |
| `platform/` | the Linux glue: build scripts, the platform patch series, package files (see its README) |
| `onboarding/` | slot for the Atlas first-run page (placeholder until AT-15) |
| `drop.list` | inherited patches left out; leaf patches only (empty) |
| `debloat.toml` | one entry per kept build-flag removal with its measured saving (AT-4) |
| `tools/atlas-series` | combined view of the three series; `--dry-run TREE` on a pristine prepared tree |
| `tests/` | shell tests of the tools (`tests/test_series.sh`) |

The build applies, in order: the inherited series (`patches/`), the platform series
(`platform/patches/`), the overlay series (`patches/` here), all through `utils/patches.py`.
`tools/apply-overlay.sh --dry-run TREE` checks the overlay alone against a tree that already
carries the first two; `--full` checks all three against a pristine tree.

## Rules

- **Budget: 60 overlay patches** and 2,500 added C++ lines at most. Every patch is a recurring cost
  at every engine release. A patch that fails to apply for two milestones in a row is a candidate
  for deletion, not for heroics.
- **Cheapest mechanism first.** In this order: a GN argument (`flags.atlas.gn`); a policy or a
  first-run preference (shipped by the WolOS system package, not here); a launcher flag; a
  **scripted transform** with asserted match counts (renames and string substitutions: they
  survive context shifts that break patches); a patch of one to a few lines; a new component.
  A patch must say in its header why a lower tier was not enough.
- **Transforms over patches** for anything mechanical (brand strings, URL scheme, hosts): a script
  in `transform/` with the expected number of matches; zero matches fails the build.
- **Never edit an inherited patch.** To change what an inherited patch does, add an overlay patch
  after it; to remove one, list it in `drop.list` (leaf patches only, checked by `atlas-series`).
- **One commit per patch.** The commit adds or changes exactly one patch file and its line in
  `patches/series`; the commit message is the patch header.

## Patch header

Every overlay patch starts with this header (plain text before the first `---` line; `patch`
ignores it):

```
Atlas overlay patch NNNN: <area>/<name>
Why: <what it changes for the person using Atlas, and why a lower tier was not enough>
Files: <engine paths>
Test filter: <gtest filter and/or journey id>
Rebase risk: L|M|H (<why>)
Release: 0.13|0.14
License: GPL-3.0-or-later (hunks in files the inherited series patches) | BSD-3-Clause (new files)
Verified at: <engine version>, prepared tree with the inherited and platform series applied
```

## Adding a patch

1. Base tree: a pristine prepared source tree with the inherited and platform series applied,
   `python3 utils/patches.py apply TREE patches atlas/platform/patches` (the build's patch stage
   does the same plus the overlay). Then `tools/apply-overlay.sh TREE` adds the current overlay.
2. Change the engine files in the tree; `diff -u` (labels `a/<path>` and `b/<path>`) into
   `atlas/patches/<area>/NNNN-name.patch`, with the header above.
3. Append the path to `atlas/patches/series` (order matters: a later patch may build on an earlier one).
4. Check on a fresh base tree: `tools/apply-overlay.sh --dry-run BASE` prints `ok` for every patch
   (or `tools/apply-overlay.sh --dry-run --full PRISTINE` for all three series).
5. One commit: `git add atlas/patches && git commit` with the header as the message.
