// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 The Atlas Authors.

#ifndef COMPONENTS_ATLAS_PAGES_SETUP_SETUP_PREFS_H_
#define COMPONENTS_ATLAS_PAGES_SETUP_SETUP_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace atlas_pages::setup {

// Whether the built-in tracker blocker is loaded. On unless the person turned it off on the
// first-run page (or later in settings). It is read when the profile starts: the blocker is a
// built-in component, and a component cannot be switched off for good any other way (its
// disable state is cleared at every start).
inline constexpr char kTrackerBlocking[] = "atlas.tracker_blocking";

// Registers the profile prefs of this area. Called by atlas::RegisterProfilePrefs()
// (chrome/browser/ui/atlas/atlas_prefs.cc).
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace atlas_pages::setup

#endif  // COMPONENTS_ATLAS_PAGES_SETUP_SETUP_PREFS_H_
