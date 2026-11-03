# atlas/platform

Linux build scripts, platform patches and package files, imported from the upstream platform
repository at its release 0.20.3.1 (GPL-3.0; the licence files are in this directory) and adapted:

- `scripts/shared.sh`, `build.sh`, `package.sh`: the fork's tree is the main repository (two levels up),
  paths and the job count come from `ATLAS_*` variables (see `scripts/shared.sh` head), every stage
  is timed into `$ATLAS_TIMINGS`, the packager makes the tarball only (no installer image) and has no
  dependency on `pv`/`zip`.
- `patches/`: the platform series (7 entries); `flags.atlas.linux.gn`: Linux GN arguments.
- `downloads.ini`, `revision.txt`, `package/`: unchanged, except that the packager reads them here.

Nothing outside `atlas/` is changed on the `atlas` branch. Attribution and licence text for the
upstream work: `docs/THIRD-PARTY.md` in the WolOS repository.
