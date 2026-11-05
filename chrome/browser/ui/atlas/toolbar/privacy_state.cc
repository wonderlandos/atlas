// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/toolbar/privacy_state.h"

#include <string_view>

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace atlas::toolbar {

namespace {

// True when `name` is registered and set, or when it is not registered.
bool ReadFlag(const PrefService& prefs, std::string_view name) {
  if (!prefs.FindPreference(name)) {
    return true;
  }
  return prefs.GetBoolean(name);
}

}  // namespace

PrivacyState ReadPrivacyState(const PrefService& prefs) {
  PrivacyState state;
  state.do_not_track = ReadFlag(prefs, ::prefs::kEnableDoNotTrack);
  state.global_privacy_control =
      ReadFlag(prefs, ::prefs::kHeliumGlobalPrivacyControl);
  state.tracker_blocking = ReadFlag(prefs, kTrackerBlockingPref);
  state.fingerprint_noise = ReadFlag(prefs, ::prefs::kHeliumNoiseEnabled);
  return state;
}

std::vector<PrivacyRow> PrivacyRows() {
  return {PrivacyRow::kDoNotTrack, PrivacyRow::kGlobalPrivacyControl,
          PrivacyRow::kTrackerBlocking, PrivacyRow::kFingerprintNoise};
}

bool IsRowOn(const PrivacyState& state, PrivacyRow row) {
  switch (row) {
    case PrivacyRow::kDoNotTrack:
      return state.do_not_track;
    case PrivacyRow::kGlobalPrivacyControl:
      return state.global_privacy_control;
    case PrivacyRow::kTrackerBlocking:
      return state.tracker_blocking;
    case PrivacyRow::kFingerprintNoise:
      return state.fingerprint_noise;
  }
}

}  // namespace atlas::toolbar
