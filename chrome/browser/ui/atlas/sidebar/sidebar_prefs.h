// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_SIDEBAR_SIDEBAR_PREFS_H_
#define CHROME_BROWSER_UI_ATLAS_SIDEBAR_SIDEBAR_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace atlas::sidebar {

// Registers the profile prefs of this area (owner: ATLAS-SIDEBAR). Called by
// atlas::RegisterProfilePrefs() (chrome/browser/ui/atlas/atlas_prefs.cc).
// A pref the engine already registers is not registered again here: change
// its default with registry->SetDefaultPrefValue() instead.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace atlas::sidebar

#endif  // CHROME_BROWSER_UI_ATLAS_SIDEBAR_SIDEBAR_PREFS_H_
