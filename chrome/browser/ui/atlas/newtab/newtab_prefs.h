// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_NEWTAB_NEWTAB_PREFS_H_
#define CHROME_BROWSER_UI_ATLAS_NEWTAB_NEWTAB_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace atlas::newtab {

namespace pref_names {
// The pinned pages of the new tab page: a list of {"title": string,
// "url": string} dictionaries, in the order the cards are shown. A new profile
// gets the default list from the first-run preferences of the system package
// (initial_preferences), so the list of a fresh profile is whatever the system
// ships; with none shipped it is empty.
inline constexpr char kPinned[] = "atlas.newtab.pinned";
}  // namespace pref_names

// Registers the preferences of the new tab page. Called by
// atlas::RegisterProfilePrefs().
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace atlas::newtab

#endif  // CHROME_BROWSER_UI_ATLAS_NEWTAB_NEWTAB_PREFS_H_
