# atlas/onboarding

Slot for the Atlas first-run page (work package AT-15, patch P-17 `ui/first-run-page`, 0.14).

The page is written in the WolOS tree and lands here as source files plus one overlay patch that
registers it as the browser's first-run page in place of the inherited one. Until then the
inherited setup page is skipped by a first-run preference that the WolOS system package ships
(`initial_preferences`), and this directory holds no build input.
