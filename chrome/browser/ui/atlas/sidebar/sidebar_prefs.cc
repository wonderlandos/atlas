// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/sidebar/sidebar_prefs.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace atlas::sidebar {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // The page gets the room (reel scene 3, pose atlas-ntp-light): the page meets
  // the sidebar and the toolbar at their 1 px lines, not inset as a rounded
  // card. A person can still turn the rounded frame on in Settings.
  registry->SetDefaultPrefValue(::prefs::kHeliumRoundedFrame,
                                base::Value(false));
}

}  // namespace atlas::sidebar
