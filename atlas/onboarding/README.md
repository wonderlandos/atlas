# Atlas first run

The page Atlas opens the first time a person starts it. It has four screens, one primary action on each, and it can be skipped from every screen but the last. It is a self-contained internal page: HTML, CSS and a small script, no network, no framework, the WolOS theme variables only. In the fork overlay this directory becomes `atlas/onboarding/` (plan-atlas.md section 1 naming, AT-15).

| Screen | What it says | Controls |
|---|---|---|
| 1. Welcome | The Atlas badge (direction B, Folio), "Welcome to Atlas", "Just use it." | Get started; Skip setup |
| 2. Private from the start | Already on: Do Not Track, Global Privacy Control, tracker blocking. Facts: no usage data, no account. | three switches, on; Back, Continue, Skip setup |
| 3. Make it yours | Light and dark follow the system, the look follows the WolOS theme (with a sample drawn from it), keyboard first, five keys | Back, Continue, Skip setup |
| 4. Ready to start | Bring things over from another browser, or start clean; "Make Atlas the default browser" only when it is not | radio pair, switch (off); Back, Start browsing / Choose a browser… |

Skip keeps what is already set: the privacy defaults stay on, nothing is imported, the default browser is left alone.

## Files

| File | What |
|---|---|
| `index.html` | the four screens; the badge and 11 glyphs as an inline sprite (`<symbol>`); a CSP meta (`default-src 'none'`, scripts and styles from the page itself, Trusted Types) |
| `onboarding.css` | layout, motion, states; colours are `var(--token)` only |
| `onboarding.js` | step logic, focus, switches, hand-over to the host. No `innerHTML`, no inline handlers |
| `host.js` | the preview host. Inside Atlas the engine serves its own adapter under this name (below) |
| `theme.css` | preview tokens, generated from `wol-light.json` / `wol-dark.json` and `shared::theme::SCALE`. Inside Atlas the engine serves the live theme under this name |
| `tools/gen_theme.py` | writes or `--check`s `theme.css` from a WolOS tree |
| `tools/mark.py` | inlines the badge SVG into the sprite (or `--check`s it) |
| `tools/check.py` | static checks: HTML structure and references, no network, no colour literals, HIG type sizes and motion tokens, no upstream or other browser named, contrast of the token pairs |
| `tools/capture.mjs` | headless run: keyboard-only walk, captures, rendered contrast, request log, motion and reduced motion, narrow window, accessibility tree |
| `preview/` | captures for review (`atlas-onboard-sheet.png` has all eight) |

```
python3 -I tools/gen_theme.py ../../split/wolos --check
python3 -I tools/mark.py ../atlas-brand/brand/atlas/directions/atlas-b-folio-128.svg --check
python3 -I tools/check.py
python3 -I -m http.server 8471 --bind 127.0.0.1 --directory . &   # stop it afterwards
PLAYWRIGHT_CORE=<playwright-core dir> CHROME=<browser binary> node tools/capture.mjs http://127.0.0.1:8471/index.html preview
```

Serve it over http to look at it: under `file://` the CSP's `'self'` does not cover the scripts.

## How the patch series wires it

What the inherited series does today (fork patch `core/onboarding-page`, seen in the clone at f29a259):

- `chrome/browser/ui/startup/startup_browser_creator_impl.cc`: on a normal (not incognito, not guest) launch without `--no-first-run`, if the profile pref `helium.completed_onboarding` (`prefs::kHeliumDidOnboarding`) is false, the only startup tab is `kHeliumSetupURL` (`chrome://setup`, shown to people as `helium://setup`; host `kHeliumSetupHost` = `setup`, in `chrome/common/webui_url_constants.{h,cc}`).
- `chrome/browser/ui/startup/first_run_service.cc`: `IsFirstRunEligibleProcess()` returns false, so the engine's own first-run experience never runs.
- `chrome/browser/ui/webui/onboarding/onboarding_page_ui.cc`: the WebUI controller for that host serves the bundle `components/helium_onboarding` (a release tarball fetched at build time, `deps.ini`; T-06 in patches-0.md) with `OnboardingMessageHandler`, `SearchEnginesHandler`, `DefaultBrowserHandler` and the import-data handler.

What Atlas does with it:

1. Tier 0, now (patches-0.md A4, A5): `initial_preferences` sets `helium.completed_onboarding: true`, so the inherited page never shows; the `distribution` keys (`skip_first_run_ui`, `suppress_first_run_default_browser_prompt`, `import_*: false`, `show_welcome_page: false`) keep the engine's prompts away. No page appears on first run.
2. P-17 `ui/first-run-page` (0.14): new `components/atlas_onboarding/` (BUILD.gn, a `.grd` that lists `index.html`, `onboarding.css`, `onboarding.js`, `theme.css`, `host.js`), built from this directory. `onboarding_page_ui.cc` keeps the `setup` host and the startup check (smallest hunk, same startup flow) and serves our resources instead of the fork bundle; the bundle drops out of `deps.ini` and `chrome/chrome_paks.gni`. A4 is removed from `initial_preferences` in the same release, so the pref is false on a new profile and the page opens once. `--no-first-run` keeps skipping it.
3. The handler (part of P-17, C++): `getState` and `finish` (contract below). `finish` sets `enable_do_not_track`, the Global Privacy Control pref (`helium.global_privacy_control`, P-02), the tracker-blocking component on or off, the default browser through the same call `DefaultBrowserHandler` makes when asked [V], then `helium.completed_onboarding = true`, and replaces the tab with the new tab page, or with the import dialog of settings (`atlas://settings/importData` [V: verify the route in the engine tree at AT-1]) when the person chose to bring things over.
4. `theme.css` inside the engine is the live WolOS theme block from P-16 `ui/tokens-webui` (generated from `design/tokens/colors.json`), served from the same `setup` host, because the page's CSP allows styles from the page's own origin only. `host.js` is the adapter of point 3, also served from that host. Neither file is copied from here into the engine; the names are the interface.

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
