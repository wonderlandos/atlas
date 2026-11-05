# Atlas first run

The page Atlas opens the first time a person starts it. It has four screens, one primary action on each, and it can be skipped from every screen but the last. It is a self-contained internal page: HTML, CSS and a small script, no network, no framework, the WolOS theme variables only. In the fork this directory is `components/atlas_pages/setup/`; it is built into the browser and served from the `setup` host (patches 0053 and 0054).

| Screen | What it says | Controls |
|---|---|---|
| 1. Welcome | The Atlas badge (direction B, Folio), "Welcome to Atlas", "Just use it." | Get started; Skip setup |
| 2. Private from the start | Already on: Do Not Track, Global Privacy Control, tracker blocking. Facts: no usage data, no account. | three switches, on; Back, Continue, Skip setup |
| 3. Make it yours | Light and dark follow the system, the look follows the WolOS theme (with a sample drawn from it), keyboard first, five keys | Back, Continue, Skip setup |
| 4. Ready to start | Bring things over from another browser, or start clean; "Make Atlas the default browser" only when it is not | radio pair, switch (off); Back, Start browsing / Choose a browser… |

Skip keeps the privacy settings as the switches show them (on, unless the person turned one off), imports nothing and leaves the default browser alone.

## Files

| File | What |
|---|---|
| `index.html` | the four screens; the badge and 11 glyphs as an inline sprite (`<symbol>`); a CSP meta (`default-src 'none'`, scripts and styles from the page itself, Trusted Types) |
| `onboarding.css` | layout, motion, states; colours are `var(--token)` only |
| `onboarding.js` | step logic, focus, switches, hand-over to the host. No `innerHTML`, no inline handlers |
| `host.js` | the preview host. Inside Atlas the engine serves `host_webui.js` under this name |
| `host_webui.js` | the engine host: the adapter between the page and the two message handlers (below). Not loaded by the preview |
| `first_run_apply.{h,cc}` | what the choices do (skip keeps, finish writes, where the tab goes next); plain C++, no engine includes |
| `first_run_handler.{h,cc}` | the engine side: parses the page's choices, writes the preferences, opens the next page |
| `setup_prefs.{h,cc}` | `atlas_pages::setup::RegisterProfilePrefs`: the preference `atlas.tracker_blocking` |
| `setup_resources.grdp`, `BUILD.gn`, `DEPS` | the page's files for the shared pak, the targets; the page is served by `atlas_pages::AtlasPageSource` of `common/` |
| `*_unittest.cc`, `mini_gtest.h` | engine unit tests (`*AtlasFirstRun*`); the logic test also compiles alone with `mini_gtest.h` |
| `tests/host_webui.test.mjs` | the adapter against a fake page bridge |
| `theme.css` | preview tokens, generated from `wol-light.json` / `wol-dark.json` and `shared::theme::SCALE`. Inside Atlas the engine serves the live theme under this name |
| `tools/gen_theme.py` | writes or `--check`s `theme.css` from a WolOS tree |
| `tools/mark.py` | inlines the badge SVG into the sprite (or `--check`s it) |
| `tools/check.py` | static checks: HTML structure and references, no network, no colour literals, HIG type sizes and motion tokens, no upstream or other browser named, contrast of the token pairs |
| `tools/capture.mjs` | headless run: keyboard-only walk, captures, rendered contrast, request log, motion and reduced motion, narrow window, accessibility tree |
| `tools/adapter_walk.mjs` | the page with the real adapter in a headless browser and a fake engine bridge: keyboard only, what is sent and in which order, no request off the page |
| `preview/` | captures for review (`atlas-onboard-sheet.png` has all eight); the reel scene R04 is compared with these |

```
python3 -I tools/gen_theme.py ../../split/wolos --check
python3 -I tools/mark.py ../atlas-brand/brand/atlas/directions/atlas-b-folio-128.svg --check
python3 -I tools/check.py
python3 -I -m http.server 8471 --bind 127.0.0.1 --directory . &   # stop it afterwards
PLAYWRIGHT_CORE=<playwright-core dir> CHROME=<browser binary> node tools/capture.mjs http://127.0.0.1:8471/index.html preview
```

Serve it over http to look at it: under `file://` the CSP's `'self'` does not cover the scripts.

All of it at once (`atlas/tests/firstrun.sh`; the browser parts need `PLAYWRIGHT_CORE` and `CHROME`):

```
PLAYWRIGHT_CORE=<playwright-core dir> CHROME=<browser binary> bash atlas/tests/firstrun.sh
```

## How the patch series wires it

What the inherited series does (fork patches `core/onboarding-page`, `core/ublock-install-as-component`), unchanged by Atlas:

- `startup_browser_creator_impl.cc`: on a normal (not incognito, not guest) launch without `--no-first-run`, if the profile pref `helium.completed_onboarding` (`prefs::kHeliumDidOnboarding`) is false, the only startup tab is `kHeliumSetupURL` (`chrome://setup`, shown to people as `atlas://setup`; host `kHeliumSetupHost` = `setup`). A URL given on the command line of the very first start is not opened.
- `first_run_service.cc`: `IsFirstRunEligibleProcess()` returns false, so the engine's own first-run experience never runs and cannot race this page (confirm in the slot tree).
- `onboarding_page_ui.cc`: the WebUI controller for that host.

What Atlas changes (overlay patches of area `ui`, fragment `50-pages.series`):

| Patch | What |
|---|---|
| 0053 `ui/firstrun-page` | the controller serves this directory through `AtlasPageSource::CreateAndAdd(context, chrome::kAtlasSetupHost, "setup")` instead of the upstream bundle; the bundle's entries leave `resource_ids.spec`, `chrome_paks.gni`, `chrome/browser/BUILD.gn`, `generated_resources.grd` (and its deps list) and `ui/webui/BUILD.gn` (the pak and the resource ids of this page are the shared ones of patch 0010). `deps.ini` loses its `[onboarding]` section in the same commit (a root file of the fork, not an engine file, so not a patch). |
| 0054 `ui/firstrun-handler` | the controller carries `settings::DefaultBrowserHandler` (state and "make default", the call the settings page makes) and `atlas_setup::AtlasFirstRunHandler`; `component_loader.cc` skips the blocker component when `atlas.tracker_blocking` is false. |
| 0055 `ui/credits-licence-files` | keeps the credits step whole after the bundle is gone (two README entries pointed into the bundle). |

`finish` sets `enable_do_not_track`, `helium.global_privacy_control` and `atlas.tracker_blocking` as chosen, then `helium.completed_onboarding`, then replaces the tab with `chrome://newtab/` or, when the person chose to bring things over, `chrome://settings/importData`. A skip writes the same three privacy values as the switches show them, imports nothing, leaves the default browser alone, and ends on the new tab page. The blocker is a built-in component whose disable state the engine clears at every start, so "off" is the preference (read where the component is added) plus an unload now.

`initial_preferences` must not set `helium.completed_onboarding` (the old tier-0 line A4 in patches-0.md): that is the WolOS side (ATLAS-WOLOS / ATLAS-PKG), it has to land with these patches or the page never opens. `--no-first-run` keeps skipping the page.

`theme.css` inside the engine is the live WolOS theme block (P-16 `ui/tokens-webui`), served from the same `setup` host because the page's CSP allows styles from its own origin only; `AtlasPageSource` writes it per request (`common/atlas_theme_css.h`; round 1: the built-in tokens). `theme.css` and `host.js` of this directory are the preview's and are not in `setup_resources.grdp`; `host_webui.js` is listed there as `setup/host.js`. The adapter carries its own 20-line response/listener bridge (the CSP lets the page load only `host.js` and `onboarding.js`, and the preview never has the shared one). The names are the interface.

### The host contract

`window.atlasFirstRun`, defined by `host.js` before `onboarding.js` runs:

| Member | Shape |
|---|---|
| `getState()` | `Promise<{ isDefault, canBeDefault, reducedMotion: true / false / null, privacy: { doNotTrack, globalPrivacyControl, trackerBlocking } }>` (booleans). The page shows the default-browser row only when `!isDefault && canBeDefault`. `reducedMotion: true` turns motion off when the portal does not map Reduce motion to the media query (motion.md section 4, [verify]). |
| `finish(choices)` | `choices = { skipped, privacy: { doNotTrack, globalPrivacyControl, trackerBlocking }, start: "import" / "clean", makeDefault }`. Called once. |
| `preview` | `true` in the preview host only. |

## What the copy promises, and what has to be true first

The page is honest only if these hold in the build that ships it; each is a release gate for P-17, not a nice-to-have.

| Line | Must be true | Gate |
|---|---|---|
| "Tracker blocking ... stops known trackers and ads" | lists present with services off (the fork's lists come from its services host) | AT-16, journey J-A10 |
| "Atlas sends no statistics and no crash reports" | services off, crash upload never, metrics policies | zero-egress test (AT-7, ADR 0043 risk 8) |
| "There is nothing to sign in to" | sync and sign-in off, vault sync hidden | `SyncDisabled`, `BrowserSignin=0` policies (plan-atlas.md, the cloud-sync row of the debloat table) |
| Do Not Track, Global Privacy Control "on already" | P-01, P-02 defaults | J-A2 |
| Super L / T / W / F / comma | Atlas's keymap; Super T and Super L are [verify] in BRIEF.md section 7, and the new-tab mock shows Ctrl T | string and keymap review |
| "In some places the law requires sites to honour it" | true for some jurisdictions; wording for the string review | string review |

## Rules it follows

- Tokens only (hig.md section 2): colours are theme variables; the one derived value is `--accent-filled`, the accent mixed 80% with black, because the system pair `accentText` on `accent` is 4.35:1 (light) and 3.69:1 (dark), under 4.5:1 for a 13 px label (tokens.md section 7). A filled-control token, if the system adds one, replaces that line.
- Motion (motion.md): card on `slow`, rows on `normal` 30 ms apart (cap 300 ms), `transform` and `opacity` only, nothing holds input; `prefers-reduced-motion: reduce` and the `reduce-motion` class jump to the end state.
- Keyboard: every control is a native button, switch (`role="switch"`) or radio; on each step focus moves to the screen's heading so a screen reader reads it; the primary action is the last stop of each screen.
- Copy (hig.md section 3): sentence case, no exclamation marks, "…" when the action asks for more input, no other browser named, no superlatives.
- Doctrine: the page and its code name no upstream project and no other browser (`tools/check.py`, section 4). This README names inherited paths and pref keys because the patches need exact names; it is a working file of the overlay, like patches-0.md.
