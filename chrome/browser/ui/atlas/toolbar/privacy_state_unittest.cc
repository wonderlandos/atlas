// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/toolbar/privacy_state.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace atlas::toolbar {

namespace {

class AtlasToolbarPrivacyStateTest : public testing::Test {
 protected:
  void RegisterAll(bool dnt, bool gpc, bool trackers, bool noise) {
    prefs_.registry()->RegisterBooleanPref(::prefs::kEnableDoNotTrack, dnt);
    prefs_.registry()->RegisterBooleanPref(::prefs::kHeliumGlobalPrivacyControl,
                                           gpc);
    prefs_.registry()->RegisterBooleanPref(kTrackerBlockingPref, trackers);
    prefs_.registry()->RegisterBooleanPref(::prefs::kHeliumNoiseEnabled, noise);
  }

  TestingPrefServiceSimple prefs_;
};

TEST_F(AtlasToolbarPrivacyStateTest, AllOnByDefault) {
  RegisterAll(true, true, true, true);
  const PrivacyState state = ReadPrivacyState(prefs_);
  EXPECT_TRUE(state.do_not_track);
  EXPECT_TRUE(state.global_privacy_control);
  EXPECT_TRUE(state.tracker_blocking);
  EXPECT_TRUE(state.fingerprint_noise);
}

TEST_F(AtlasToolbarPrivacyStateTest, EachPrefMapsToItsOwnRow) {
  RegisterAll(true, true, true, true);
  prefs_.SetBoolean(::prefs::kEnableDoNotTrack, false);
  PrivacyState state = ReadPrivacyState(prefs_);
  EXPECT_FALSE(IsRowOn(state, PrivacyRow::kDoNotTrack));
  EXPECT_TRUE(IsRowOn(state, PrivacyRow::kGlobalPrivacyControl));
  EXPECT_TRUE(IsRowOn(state, PrivacyRow::kTrackerBlocking));
  EXPECT_TRUE(IsRowOn(state, PrivacyRow::kFingerprintNoise));

  prefs_.SetBoolean(::prefs::kEnableDoNotTrack, true);
  prefs_.SetBoolean(::prefs::kHeliumGlobalPrivacyControl, false);
  state = ReadPrivacyState(prefs_);
  EXPECT_FALSE(IsRowOn(state, PrivacyRow::kGlobalPrivacyControl));
  EXPECT_TRUE(IsRowOn(state, PrivacyRow::kDoNotTrack));

  prefs_.SetBoolean(::prefs::kHeliumGlobalPrivacyControl, true);
  prefs_.SetBoolean(kTrackerBlockingPref, false);
  state = ReadPrivacyState(prefs_);
  EXPECT_FALSE(IsRowOn(state, PrivacyRow::kTrackerBlocking));

  prefs_.SetBoolean(kTrackerBlockingPref, true);
  prefs_.SetBoolean(::prefs::kHeliumNoiseEnabled, false);
  state = ReadPrivacyState(prefs_);
  EXPECT_FALSE(IsRowOn(state, PrivacyRow::kFingerprintNoise));
}

TEST_F(AtlasToolbarPrivacyStateTest, UnregisteredPrefReadsOn) {
  // Only Do Not Track exists: the others read as their default, on.
  prefs_.registry()->RegisterBooleanPref(::prefs::kEnableDoNotTrack, false);
  const PrivacyState state = ReadPrivacyState(prefs_);
  EXPECT_FALSE(state.do_not_track);
  EXPECT_TRUE(state.global_privacy_control);
  EXPECT_TRUE(state.tracker_blocking);
  EXPECT_TRUE(state.fingerprint_noise);
}

TEST_F(AtlasToolbarPrivacyStateTest, RowsInTheOrderOfThePopover) {
  const std::vector<PrivacyRow> rows = PrivacyRows();
  ASSERT_EQ(4u, rows.size());
  EXPECT_EQ(PrivacyRow::kDoNotTrack, rows[0]);
  EXPECT_EQ(PrivacyRow::kGlobalPrivacyControl, rows[1]);
  EXPECT_EQ(PrivacyRow::kTrackerBlocking, rows[2]);
  EXPECT_EQ(PrivacyRow::kFingerprintNoise, rows[3]);
}

}  // namespace

}  // namespace atlas::toolbar
