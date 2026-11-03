# atlas/platform

Linux build scripts, platform patches and package files, imported from the upstream platform
repository at its release 0.20.3.1 (GPL-3.0; the licence files are in this directory) and adapted:

- `scripts/shared.sh`, `build.sh`, `package.sh`: the fork's tree is the main repository (two levels up),
  paths and the job count come from `ATLAS_*` variables (see `scripts/shared.sh` head), every stage
  is timed into `$ATLAS_TIMINGS`, the packager makes the tarball only (no installer image) and has no
  dependency on `pv`/`zip`.
- `patches/`: the platform series (7 entries); `flags.atlas.linux.gn`: Linux GN arguments.
- `downloads.ini`, `revision.txt`: unchanged; `package/`: the desktop file, wrapper and AppArmor
  profile under the Atlas names (`atlas.desktop`, `atlas-wrapper`, profile `atlas-bin` for
  `/opt/atlas/atlas`); the AppImage wrapper is gone (we ship the tarball only).
- Names (plan V-01..V-03): binary `atlas`, crash handler `atlas_crashpad_handler`, user data
  `~/.config/atlas`, environment `ATLAS_CONFIG_HOME` and `ATLAS_DESKTOP`, desktop file
  `atlas.desktop`, icon name `atlas`, installer metadata; the platform patches live in
  `patches/linux/` and are edited in place (this small Linux-only set is ours to edit).

These files are the only copies the build uses. Attribution and licence text for the
upstream work: `docs/THIRD-PARTY.md` in the WolOS repository.
