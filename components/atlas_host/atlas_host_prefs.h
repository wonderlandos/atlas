// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATLAS_HOST_ATLAS_HOST_PREFS_H_
#define COMPONENTS_ATLAS_HOST_ATLAS_HOST_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace atlas_host {

// Registers the profile prefs of this area (owner: ATLAS-HOST). Called by
// atlas::RegisterProfilePrefs() (chrome/browser/ui/atlas/atlas_prefs.cc).
// A pref the engine already registers is not registered again here: change
// its default with registry->SetDefaultPrefValue() instead.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace atlas_host

#endif  // COMPONENTS_ATLAS_HOST_ATLAS_HOST_PREFS_H_
