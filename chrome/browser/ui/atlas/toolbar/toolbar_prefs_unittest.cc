// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/toolbar/toolbar_prefs.h"

#include <optional>
#include <string>

#include "base/values.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

namespace atlas::toolbar {

namespace {

class AtlasToolbarPrefsTest : public testing::Test {
 protected:
  AtlasToolbarPrefsTest() {
    // The engine registers these with its own defaults before the Atlas registrar runs.
    auto* registry = prefs_.registry();
    registry->RegisterBooleanPref(::prefs::kShowAvatarButton, true);
    registry->RegisterBooleanPref(::prefs::kShowExtensionsButton, true);
    registry->RegisterBooleanPref(::prefs::kShowDynamicNewTabButton, true);
    registry->RegisterBooleanPref(::prefs::kShowHomeButton, false);
    registry->RegisterBooleanPref(::prefs::kShowForwardButton, true);
    registry->RegisterBooleanPref(::prefs::kShowMenuButton, true);
    registry->RegisterBooleanPref(::prefs::kShowVerticalTabsCollapseButton,
                                  true);
    base::ListValue labs;
    labs.Append("kActionShowChromeLabs");
    registry->RegisterListPref(::prefs::kPinnedActions, std::move(labs));
    RegisterProfilePrefs(registry);
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(AtlasToolbarPrefsTest, ButtonsOfTheReelAreOnForANewProfile) {
  EXPECT_TRUE(prefs_.GetBoolean(kShowPrivacyButton));
  EXPECT_TRUE(prefs_.GetBoolean(kShowBookmarkButton));
  EXPECT_TRUE(prefs_.GetBoolean(::prefs::kShowForwardButton));
  EXPECT_TRUE(prefs_.GetBoolean(::prefs::kShowMenuButton));
  EXPECT_TRUE(prefs_.GetBoolean(::prefs::kShowVerticalTabsCollapseButton));
}

TEST_F(AtlasToolbarPrefsTest, ButtonsNotInTheReelAreOffForANewProfile) {
  EXPECT_FALSE(prefs_.GetBoolean(::prefs::kShowAvatarButton));
  EXPECT_FALSE(prefs_.GetBoolean(::prefs::kShowExtensionsButton));
  EXPECT_FALSE(prefs_.GetBoolean(::prefs::kShowDynamicNewTabButton));
  EXPECT_FALSE(prefs_.GetBoolean(::prefs::kShowHomeButton));
}

TEST_F(AtlasToolbarPrefsTest, ThePersonCanStillChangeADefault) {
  prefs_.SetBoolean(::prefs::kShowAvatarButton, true);
  EXPECT_TRUE(prefs_.GetBoolean(::prefs::kShowAvatarButton));
  prefs_.SetBoolean(kShowPrivacyButton, false);
  EXPECT_FALSE(prefs_.GetBoolean(kShowPrivacyButton));
}

TEST_F(AtlasToolbarPrefsTest, DownloadsIsPinnedByDefaultAfterTheEnginesOwnEntries) {
  // The action names exist only where the action manager is initialised: elsewhere nothing is
  // pinned and the list stays as the engine left it.
  const std::optional<std::string>& downloads =
      actions::ActionIdMap::ActionIdToString(kActionShowDownloads);
  const base::ListValue& pinned = prefs_.GetList(::prefs::kPinnedActions);
  ASSERT_FALSE(pinned.empty());
  EXPECT_EQ("kActionShowChromeLabs", pinned[0].GetString());
  if (downloads.has_value()) {
    ASSERT_EQ(2u, pinned.size());
    EXPECT_EQ(*downloads, pinned[1].GetString());
  } else {
    EXPECT_EQ(1u, pinned.size());
  }
}

}  // namespace

}  // namespace atlas::toolbar
