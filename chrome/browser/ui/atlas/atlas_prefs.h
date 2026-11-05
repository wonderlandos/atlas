// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_ATLAS_PREFS_H_
#define CHROME_BROWSER_UI_ATLAS_ATLAS_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace atlas {

// Registers the profile prefs of every Atlas area. Called once, from the end
// of RegisterProfilePrefs() in chrome/browser/prefs/browser_prefs.cc (anchor
// patch 0010). Each area registers its own prefs in its own
// RegisterProfilePrefs(); an area never edits browser_prefs.cc.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace atlas

#endif  // CHROME_BROWSER_UI_ATLAS_ATLAS_PREFS_H_
