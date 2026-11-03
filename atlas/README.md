# atlas/

Overlay on the inherited patch series. Nothing outside this directory differs from the `upstream`
branch (check: `git diff --stat upstream..atlas -- . ':!atlas'` prints nothing).

| Path | What |
|---|---|
| `platform/` | Linux build scripts, platform patches, GN arguments (see its README) |
| `patches/series` | our patches, applied after the inherited series and the platform series (empty at AT-1) |
| `drop.list` | inherited patches to leave out; leaf patches only (empty at AT-1) |
| `debloat.toml` | one line per kept build-flag removal with its measured saving (AT-4) |
| `tools/atlas-series` | combined view of the three series, `--dry-run` on a prepared tree |
| `tests/` | shell tests, run by `tests/test_series.sh` |

Branches: `upstream` is a plain copy of the upstream repository at the pinned release;
`atlas` is `upstream` plus this directory. Updating = fetch, `git merge upstream`.
