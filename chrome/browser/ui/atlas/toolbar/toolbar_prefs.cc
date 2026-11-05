// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/toolbar/toolbar_prefs.h"

#include <optional>
#include <string>
#include <utility>

#include "base/values.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_store.h"
#include "ui/actions/actions.h"

namespace atlas::toolbar {

namespace {

// The toolbar of the reel has no avatar, no extensions button and no new-tab button (the sidebar
// has a New tab row); the sidebar toggle, forward and the menu stay on. (The centred omnibox
// is ATLAS-CHROME's default, in patch 0071.)
void SetEngineButtonDefaults(user_prefs::PrefRegistrySyncable* registry) {
  registry->SetDefaultPrefValue(::prefs::kShowAvatarButton, base::Value(false));
  registry->SetDefaultPrefValue(::prefs::kShowExtensionsButton,
                                base::Value(false));
  registry->SetDefaultPrefValue(::prefs::kShowDynamicNewTabButton,
                                base::Value(false));
  registry->SetDefaultPrefValue(::prefs::kShowHomeButton, base::Value(false));
  registry->SetDefaultPrefValue(::prefs::kShowForwardButton,
                                base::Value(true));
  registry->SetDefaultPrefValue(::prefs::kShowMenuButton, base::Value(true));
  registry->SetDefaultPrefValue(::prefs::kShowVerticalTabsCollapseButton,
                                base::Value(true));
}

// Pins the Downloads button by default so it is always on the toolbar, in the place the reel
// shows it, instead of appearing after the first download.
void PinDownloadsByDefault(user_prefs::PrefRegistrySyncable* registry) {
  // The action names are not available in unit tests that do not build the
  // action manager: then there is nothing to pin.
  const std::optional<std::string>& downloads =
      actions::ActionIdMap::ActionIdToString(kActionShowDownloads);
  if (!downloads.has_value()) {
    return;
  }

  base::ListValue pinned;
  const base::Value* current = nullptr;
  if (registry->defaults()->GetValue(::prefs::kPinnedActions, &current) &&
      current->is_list()) {
    pinned = current->GetList().Clone();
  }
  for (const base::Value& item : pinned) {
    if (item.is_string() && item.GetString() == *downloads) {
      return;
    }
  }
  pinned.Append(*downloads);
  registry->SetDefaultPrefValue(::prefs::kPinnedActions,
                                base::Value(std::move(pinned)));
}

}  // namespace

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kShowPrivacyButton, true);
  registry->RegisterBooleanPref(kShowBookmarkButton, true);

  SetEngineButtonDefaults(registry);
  PinDownloadsByDefault(registry);
}

}  // namespace atlas::toolbar
