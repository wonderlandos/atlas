// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_TOOLBAR_PRIVACY_STATE_H_
#define CHROME_BROWSER_UI_ATLAS_TOOLBAR_PRIVACY_STATE_H_

#include <vector>

class PrefService;

namespace atlas::toolbar {

// What the privacy button and its popover say, read from the profile prefs. Nothing here changes
// a pref: in this release the popover only reports, and the person changes a setting in Settings.
struct PrivacyState {
  // The request that asks sites not to follow the person (prefs::kEnableDoNotTrack). Sites may
  // ignore it, so no text says that anything is "blocked" because of it.
  bool do_not_track = true;
  // Global Privacy Control (prefs::kHeliumGlobalPrivacyControl).
  bool global_privacy_control = true;
  // The list-based tracker blocker built into the browser ("atlas.tracker_blocking").
  bool tracker_blocking = true;
  // Fingerprint noise on canvas and audio reads (prefs::kHeliumNoiseEnabled).
  bool fingerprint_noise = true;
};

// The pref of the tracker blocker, owned by the first-run area
// (components/atlas_pages/setup/setup_prefs.h, kTrackerBlocking). It is repeated here as text so
// this area does not depend on the page code. Keep the two in step (atlas/tests/toolbar.sh checks it).
inline constexpr char kTrackerBlockingPref[] = "atlas.tracker_blocking";

// Reads the state. A pref that is not registered (a profile of a kind that has no such pref)
// reads as its default, which is on.
PrivacyState ReadPrivacyState(const PrefService& prefs);

// One row of the popover: the setting, one line of what it does, and whether it is on.
enum class PrivacyRow {
  kDoNotTrack,
  kGlobalPrivacyControl,
  kTrackerBlocking,
  kFingerprintNoise,
};

// The rows in the order the popover shows them.
std::vector<PrivacyRow> PrivacyRows();

// Whether `row` is on in `state`.
bool IsRowOn(const PrivacyState& state, PrivacyRow row);

}  // namespace atlas::toolbar

#endif  // CHROME_BROWSER_UI_ATLAS_TOOLBAR_PRIVACY_STATE_H_
