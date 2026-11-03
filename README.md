# Atlas

The web browser of WolOS: private by default, quiet, and honest about what it does.

## Standing on good work

This repository is where Atlas, the browser of WolOS, is built. Atlas starts
from [Helium](https://github.com/imputnet/helium), made by the
[imput](https://github.com/imputnet) team, and from the patch series in this
tree.

Their work carried the browser a long way toward what WolOS wants from one:
privacy settings that start on the person's side, no account to create and
nothing that keeps asking for one, no usage data sent home, and an interface
that does not claim more than it does. We did not have to begin that work; we
began from it.

Atlas adds its own changes as a separate layer on top, so the base stays
theirs and stays easy to follow. When a fix of ours belongs in the base, we
offer it to them. The credit for the base is theirs, and so are the licence
terms below, which this fork keeps as they are.

## What this repository is

The browser source (Chromium) is far too large to be a git repository, so this repository is the
patch set that turns a prepared Chromium source tree into Atlas:

- branch `upstream`: an exact mirror of Helium at its latest release tag, only ever
  fast-forwarded, never edited;
- branch `main`: `upstream` plus the Atlas overlay, a short straight line of our own commits, one
  commit per change, rebased onto each new upstream release.

| Path | What |
|---|---|
| `patches/` | the inherited patch series, as published upstream (never edited here) |
| `atlas/` | the overlay: our patches, GN arguments, Linux platform glue, first-run slot ([atlas/README.md](atlas/README.md)) |
| `tools/apply-overlay.sh` | apply or dry-run the overlay series against a prepared tree |
| `tools/sync-upstream.sh` | fetch upstream, fast-forward `upstream`, replay the overlay, report conflicts per patch |
| everything else | inherited tooling (`utils/`, `devutils/`, `resources/`, lists), used by the build as it is |

Atlas is built on the WolOS build forge, not on hosted CI runners: `atlas/platform/scripts/build.sh`
prepares, patches and compiles the tree, `atlas/platform/scripts/package.sh` makes the tarball the
WolOS `atlas-browser` package repackages.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) first. Changes to the browser go in `atlas/` as one patch per
commit; nothing under `patches/` is edited.

## Credits

### The Chromium project
[The Chromium Project](https://www.chromium.org/) is at the core of Atlas, as it is of Helium,
making it possible in the first place.

### ungoogled-chromium
Helium originally branched off
[ungoogled-chromium](https://github.com/ungoogled-software/ungoogled-chromium), and its patches
are part of the inherited series.

### Other Chromium projects
The inherited series includes patches from other open source Chromium projects:

- [Inox patchset](https://github.com/gcarq/inox-patchset)
- [Debian](https://tracker.debian.org/pkg/chromium-browser)
- [Bromite](https://github.com/bromite/bromite)
- [Iridium Browser](https://iridiumbrowser.de/)
- [Brave](https://github.com/brave/brave-core)

All inherited patches are sorted by vendor in the [patches](patches/) directory.

## License

Upstream's notice, kept: all code, patches, modified portions of imported code or patches, and
any other content that is unique to Helium and not imported from other repositories is licensed
under GPL-3.0. See [LICENSE](LICENSE). Any content imported from other projects retains its
original license (for example, any original unmodified code imported from ungoogled-chromium
remains licensed under their [BSD 3-Clause license](LICENSE.ungoogled_chromium)).

The Atlas overlay: patches in `atlas/` that modify files the inherited series patches, and our
changes to inherited scripts, are GPL-3.0-or-later; new source files we add to the engine tree are
BSD-3-Clause, like Chromium itself. The files in `atlas/platform/` keep the licence files of the
platform repository they come from. The Atlas name and artwork are not covered by these licences.
