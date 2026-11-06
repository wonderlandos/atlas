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
| `patches/series` | the first overlay patches (0001-0005), applied after the inherited series and the platform series; closed, new patches go in a fragment |
| `patches/series.d/NN-area.series` | one fragment per area, applied after `patches/series` in file-name order; its `# range: NNNN-NNNN` line is the numbers of its area; see "Numbering and owners" |
| `patches/<area>/NNNN-name.patch` | one overlay patch; `NNNN` is unique across areas and never reused; areas: `privacy`, `security`, `brand`, `memory`, `host`, `ui`, `debloat`, `tor`, `build`, `fed` |
| `tree.list` | the overlay tree: directories of new engine files (`chrome/app/atlas`, `chrome/browser/atlas`, `chrome/browser/ui/atlas`, `components/atlas_host`, `components/atlas_pages`) kept at their engine path in this repository and copied into the engine tree as they are |
| `flags.atlas.gn` | our GN arguments, appended after `flags.gn` and `platform/flags.atlas.linux.gn` (the last assignment wins) |
| `platform/` | the Linux glue: build scripts, the platform patch series, package files (see its README) |
| `onboarding/` | the Atlas first-run page (HTML, CSS, script, its tools and checks); not a build input until patch P-17 wires it in (0.14, see its README) |
| `drop.list` | inherited patches left out; leaf patches only (one entry: the credential block, see `60-vault.series`) |
| `debloat.toml` | one entry per kept build-flag removal with its measured saving (AT-4) |
| `tools/atlas-series` | combined view of the series: `list`, `overlay`, `check`, `write-series DIR`, `install-tree TREE`, `--dry-run TREE` on a pristine prepared tree |
| `tests/` | shell tests of the tools and of the overlay (`tests/test_series.sh`, `tests/test_tools.sh`, `tests/check_overlay.sh`) |

The build (`platform/scripts/shared.sh`, `apply_patches`) asks `atlas-series` for one combined series
and applies it through `utils/patches.py`, in this order: the inherited series (`patches/`) minus
`drop.list`, the platform series (`platform/patches/`), `patches/series`, then the fragments
`patches/series.d/*.series` in file-name order. Before the patches it copies the overlay tree
(`tree.list`) into the engine tree, so a patch may hook the new files. `atlas-series` refuses a patch
listed twice, a number used twice, an entry that names a missing file and an overlay patch that
touches a file of the overlay tree; it warns when a number is outside the range of its fragment.
`tools/apply-overlay.sh --dry-run TREE` checks the overlay alone against a tree that already
carries the first two series; `--full` checks all of them against a pristine tree.

## Numbering and owners

Overlay numbers are unique across areas. Every area has a fragment, so two authors never edit the
same series file; the fragment names the range, and an author uses only the numbers of its own row.

| Fragment | Range | Owner and content |
|---|---|---|
| `patches/series` | 0001-0005 | privacy defaults (done) |
| `05-tor.series` | 0006-0009 | ATLAS-TOR, Tor and private tabs (ADR 0053) |
| `10-build.series` | 0010 | ATLAS-ANCHOR, the anchor patch: string parts, build targets, paks, pref registrar, hosts, unit tests |
| `15-brand.series` | 0011-0019 | ATLAS-BRAND-FORK (0011, 0012); ATLAS-ANCHOR (0013, the policy directory) |
| `20-sidebar.series` | 0020-0023 | ATLAS-SIDEBAR |
| `24-rest.series` | 0024-0029 | ATLAS-REST, the resting mark of tabs |
| `30-toolbar.series` | 0030-0039 | ATLAS-TOOLBAR |
| `40-host.series` | 0040-0049 | ATLAS-HOST (bridge client, download and context-menu verbs) |
| `41-fed.series` | 0120-0129 | ATLAS-FED (fed:// in Atlas: scheme, loaders, location-bar glyph, exclusive-fed guards: no http(s) or DNS for a Federate name; helper `atlas-fed` in the WolOS repository, ADR 0052) |
| `50-pages.series` | 0050-0059 | ATLAS-NTP (0050-0052), ATLAS-FIRSTRUN (0053-0059) |
| `60-vault.series` | 0060-0069 | ATLAS-VAULT |
| `70-chrome.series` | 0070-0079 | ATLAS-CHROME (0070-0072), ATLAS-THEME (0073-0079) |
| `80-memory.series` | 0080-0089 | ATLAS-REST, memory saver defaults |
| `90-privacy.series` | 0090-0094 | ATLAS-PRIVACY, registered defaults |

### What the anchor patch gives every area (0010)

An area never edits these engine files; it works in its own files and the hooks are already there.

| Need | Where |
|---|---|
| Strings | `chrome/app/atlas/<area>_strings.grdp` (areas `sidebar`, `rest`, `toolbar`, `host`, `pages`, `vault`, `brand`), parts of the root grd |
| Code of the browser UI | `chrome/browser/ui/atlas/<area>/BUILD.gn` (`sidebar`, `rest`, `toolbar`, `newtab`), linked into `//chrome/browser/ui` which allows them to include its headers |
| Host client | `components/atlas_host` |
| Pages served by the browser | `components/atlas_pages/<page>/` (`newtab`, `setup`): files listed in `<page>_resources.grdp` with `resource_path="<page>/<file>"`, one pak (`atlas_pages_resources.pak`, ids 7560-7599); the page is served by `atlas_pages::AtlasPageSource::CreateAndAdd(browser_context, chrome::kAtlasNewTabHost or kAtlasSetupHost, "<page>")` (CSP, Trusted Types, no network, `theme.css` from the live colours) |
| Prefs | `RegisterProfilePrefs(registry)` of your own area, already called from `atlas::RegisterProfilePrefs()` (`chrome/browser/ui/atlas/atlas_prefs.cc`) from `browser_prefs.cc` |
| Unit tests | your `:unit_tests` target (testonly), collected by `//chrome/browser/ui/atlas:unit_tests` which `chrome/test/BUILD.gn` depends on |
| Hosts | `chrome::kAtlasNewTabHost` (`new-tab-page`, the engine's own host), `chrome::kAtlasSetupHost` (`setup`) |

New files are not patched in: they are plain files in the overlay tree at their engine path and are
edited directly. A patch only hooks them into engine files. The engine-shaped paths are staging for
patch generation; the rule "no edits outside `atlas/`, `tools/` and the documents" is about files of the
upstream mirror.

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
- **One commit per patch.** The commit adds or changes exactly one patch file and its line in its
  fragment; the commit message is the patch header.

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
3. Append the path to the fragment of your area, `atlas/patches/series.d/NN-area.series`, never to
   `atlas/patches/series` (order matters: a later patch may build on an earlier one; fragments are
   applied in file-name order).
4. Check on a fresh base tree: `tools/apply-overlay.sh --dry-run BASE` prints `ok` for every patch
   (or `tools/apply-overlay.sh --dry-run --full PRISTINE` for all three series).
5. One commit: `git add atlas/patches && git commit` with the header as the message.
