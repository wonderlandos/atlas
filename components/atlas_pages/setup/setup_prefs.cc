// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 The Atlas Authors.

#include "components/atlas_pages/setup/setup_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace atlas_pages::setup {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kTrackerBlocking, true);
}

}  // namespace atlas_pages::setup
