# components/atlas_pages: the pages Atlas serves itself

| Directory | Host | Owner |
|---|---|---|
| `common/` | none: shared by the pages | ATLAS-ANCHOR |
| `newtab/` | `chrome://newtab` (`chrome::kAtlasNewTabHost`) | ATLAS-NTP |
| `setup/` | `chrome://setup` (`chrome::kAtlasSetupHost`) | ATLAS-FIRSTRUN |

How a page is served:

- Its files are listed in its own `<dir>/<dir>_resources.grdp` with `resource_path="<dir>/<file>"`;
  all parts go into one pak, `atlas_pages_resources.pak` (`atlas_pages_resources.grd`).
- Its WebUI controller calls `atlas_pages::AtlasPageSource::CreateAndAdd(profile, host, "<dir>")`
  (`common/atlas_page_source.h`). The source serves `<dir>/<file>` as `chrome://<host>/<file>`,
  `<dir>/index.html` for any other path, the common files (`atlas_host_bridge.js`), and
  `theme.css`: the `:root` block of the 13 theme tokens from the theme CSS provider
  (`common/atlas_theme_css.h`). A page links `theme.css` and uses only the variables in it.
- The policy is the first-run page's: `default-src 'none'`, scripts and styles from the page itself,
  images from the page and `data:`, no network, Trusted Types with no policy
  (`AtlasPageSource::kContentSecurityPolicy`).
- Script talks to the browser only through its adapter `host.js`, built on
  `common/atlas_host_bridge.js`: `window.atlasFirstRun` for setup, `window.atlasNewTab` for the new
  tab page. The shapes are in `common/atlas_host_bridge.js`.
- Prefs register in `<dir>/<dir>_prefs.cc`, called by `atlas::RegisterProfilePrefs()`.
