// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_TOOLBAR_TOOLBAR_PREFS_H_
#define CHROME_BROWSER_UI_ATLAS_TOOLBAR_TOOLBAR_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace atlas::toolbar {

// Whether the privacy button (Do Not Track and the other privacy facts) is on the toolbar.
inline constexpr char kShowPrivacyButton[] = "atlas.toolbar.show_privacy_button";

// Whether the bookmark button is on the toolbar. While it is, the omnibox does not show its own
// star, so the page has one bookmark control, not two.
inline constexpr char kShowBookmarkButton[] = "atlas.toolbar.show_bookmark_button";

// Registers the profile prefs of this area (owner: ATLAS-TOOLBAR) and sets the default toolbar of
// a new profile: sidebar toggle, back, forward, reload, the omnibox, then privacy, bookmark,
// downloads and the menu. The engine's own toolbar prefs (helium.browser.show_*, the pinned
// actions list) are only given new defaults here; they stay the person's to change. Called by
// atlas::RegisterProfilePrefs() (chrome/browser/ui/atlas/atlas_prefs.cc), after the engine has
// registered the prefs whose defaults this changes.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace atlas::toolbar

#endif  // CHROME_BROWSER_UI_ATLAS_TOOLBAR_TOOLBAR_PREFS_H_
