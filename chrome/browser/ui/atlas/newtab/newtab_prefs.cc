// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/newtab/newtab_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace atlas::newtab {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(pref_names::kPinned);
}

}  // namespace atlas::newtab
