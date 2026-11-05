// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 The Atlas Authors.

#include "components/atlas_pages/setup/first_run_handler.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/atlas_pages/setup/setup_prefs.h"
#include "components/helium_services/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace atlas_pages::setup {
namespace {

base::DictValue Json(const char* text) {
  std::optional<base::DictValue> dict = base::JSONReader::ReadDict(text);
  CHECK(dict.has_value()) << text;
  return std::move(*dict);
}

// A fake prefs service with the three preferences the first run touches, registered the way
// the browser registers them after the privacy patches (on by default).
class AtlasFirstRunHandlerTest : public testing::Test {
 protected:
  AtlasFirstRunHandlerTest() {
    prefs_.registry()->RegisterBooleanPref(prefs::kEnableDoNotTrack, true);
    prefs_.registry()->RegisterBooleanPref(prefs::kHeliumGlobalPrivacyControl,
                                           true);
    prefs_.registry()->RegisterBooleanPref(prefs::kHeliumDidOnboarding, false);
    prefs_.registry()->RegisterBooleanPref(kTrackerBlocking, true);
  }

  ProfileEnvironment MakeEnvironment() {
    return ProfileEnvironment(
        &prefs_, base::BindRepeating(
                     [](std::vector<bool>* calls, bool on) {
                       calls->push_back(on);
                     },
                     &blocker_calls_));
  }

  TestingPrefServiceSimple prefs_;
  std::vector<bool> blocker_calls_;
};

TEST_F(AtlasFirstRunHandlerTest, ParsesWhatThePageSends) {
  auto choices = ParseChoices(Json(R"({
    "skipped": false,
    "privacy": {"doNotTrack": true, "globalPrivacyControl": false,
                "trackerBlocking": true},
    "start": "import", "makeDefault": true})"));
  ASSERT_TRUE(choices.has_value());
  EXPECT_FALSE(choices->skipped);
  EXPECT_TRUE(choices->do_not_track);
  EXPECT_FALSE(choices->global_privacy_control);
  EXPECT_TRUE(choices->tracker_blocking);
  EXPECT_EQ(Choices::Start::kImport, choices->start);
}

TEST_F(AtlasFirstRunHandlerTest, ParsesASkipWithItsPrivacySwitchesAndNoStart) {
  auto choices = ParseChoices(Json(R"({
    "skipped": true,
    "privacy": {"doNotTrack": false, "globalPrivacyControl": true,
                "trackerBlocking": true},
    "start": "import"})"));
  ASSERT_TRUE(choices.has_value());
  EXPECT_TRUE(choices->skipped);
  EXPECT_FALSE(choices->do_not_track);
  EXPECT_EQ(Choices::Start::kClean, choices->start);
}

TEST_F(AtlasFirstRunHandlerTest, RefusesAnythingElse) {
  const char* bad[] = {
      R"({})",
      R"({"skipped": "no"})",
      R"({"skipped": true})",
      R"({"skipped": true, "privacy": 5})",
      R"({"skipped": false})",
      R"({"skipped": false, "privacy": {"doNotTrack": true}, "start": "clean"})",
      R"({"skipped": false, "start": "clean", "privacy":
          {"doNotTrack": 1, "globalPrivacyControl": true,
           "trackerBlocking": true}})",
      R"({"skipped": false, "start": "later", "privacy":
          {"doNotTrack": true, "globalPrivacyControl": true,
           "trackerBlocking": true}})",
      R"({"skipped": false, "privacy":
          {"doNotTrack": true, "globalPrivacyControl": true,
           "trackerBlocking": true}})",
  };
  for (const char* text : bad) {
    EXPECT_FALSE(ParseChoices(Json(text)).has_value()) << text;
  }
}

TEST_F(AtlasFirstRunHandlerTest, StateReadsThePreferences) {
  prefs_.SetBoolean(prefs::kHeliumGlobalPrivacyControl, false);
  prefs_.SetBoolean(kTrackerBlocking, false);
  base::DictValue state = BuildState(prefs_);
  const base::DictValue* privacy = state.FindDict("privacy");
  ASSERT_TRUE(privacy);
  EXPECT_EQ(true, privacy->FindBool("doNotTrack"));
  EXPECT_EQ(false, privacy->FindBool("globalPrivacyControl"));
  EXPECT_EQ(false, privacy->FindBool("trackerBlocking"));
  EXPECT_TRUE(state.Find("reducedMotion")->is_none());
}

TEST_F(AtlasFirstRunHandlerTest, FinishWritesTheChoicesAndCompletion) {
  ProfileEnvironment env = MakeEnvironment();
  Choices choices;
  choices.do_not_track = false;
  choices.global_privacy_control = false;
  choices.tracker_blocking = false;
  EXPECT_EQ(NextPage::kNewTab, Apply(choices, env));
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kEnableDoNotTrack));
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kHeliumGlobalPrivacyControl));
  EXPECT_FALSE(prefs_.GetBoolean(kTrackerBlocking));
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kHeliumDidOnboarding));
  EXPECT_EQ(std::vector<bool>({false}), blocker_calls_);
}

TEST_F(AtlasFirstRunHandlerTest, FinishWithTheDefaultsKeepsEverythingOn) {
  ProfileEnvironment env = MakeEnvironment();
  Apply(Choices(), env);
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kEnableDoNotTrack));
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kHeliumGlobalPrivacyControl));
  EXPECT_TRUE(prefs_.GetBoolean(kTrackerBlocking));
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kHeliumDidOnboarding));
  EXPECT_EQ(std::vector<bool>({true}), blocker_calls_);
}

TEST_F(AtlasFirstRunHandlerTest, SkipKeepsTheSwitchesAsShownAndCompletes) {
  ProfileEnvironment env = MakeEnvironment();
  Choices choices;
  choices.skipped = true;
  choices.do_not_track = false;
  EXPECT_EQ(NextPage::kNewTab, Apply(choices, env));
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kEnableDoNotTrack));
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kHeliumGlobalPrivacyControl));
  EXPECT_TRUE(prefs_.GetBoolean(kTrackerBlocking));
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kHeliumDidOnboarding));
}

TEST_F(AtlasFirstRunHandlerTest, TheBlockerPreferenceIsOnByDefaultUnderItsFixedName) {
  // The component loader hook (patch 0054) reads the preference by this literal name.
  EXPECT_STREQ("atlas.tracker_blocking", kTrackerBlocking);
  sync_preferences::TestingPrefServiceSyncable syncable;
  RegisterProfilePrefs(syncable.registry());
  EXPECT_TRUE(syncable.GetBoolean(kTrackerBlocking));
}

TEST_F(AtlasFirstRunHandlerTest, TheNextPageIsTheNewTabOrTheImportPage) {
  EXPECT_EQ("chrome://newtab/", NextPageUrl(NextPage::kNewTab).spec());
  EXPECT_EQ("chrome://settings/importData",
            NextPageUrl(NextPage::kImportData).spec());
}

}  // namespace
}  // namespace atlas_pages::setup
