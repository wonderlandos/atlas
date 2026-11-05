// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/atlas_prefs.h"

#include "chrome/browser/ui/atlas/newtab/newtab_prefs.h"
#include "chrome/browser/ui/atlas/rest/rest_prefs.h"
#include "chrome/browser/ui/atlas/sidebar/sidebar_prefs.h"
#include "chrome/browser/ui/atlas/toolbar/toolbar_prefs.h"
#include "components/atlas_host/atlas_host_prefs.h"
#include "components/atlas_pages/setup/setup_prefs.h"

namespace atlas {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  sidebar::RegisterProfilePrefs(registry);
  rest::RegisterProfilePrefs(registry);
  toolbar::RegisterProfilePrefs(registry);
  atlas_host::RegisterProfilePrefs(registry);
  newtab::RegisterProfilePrefs(registry);
  atlas_pages::setup::RegisterProfilePrefs(registry);
}

}  // namespace atlas
