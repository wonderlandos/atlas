// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 The Atlas Authors.
//
// What the first-run page's choices do. This file has no engine includes on purpose: the
// rule (what a skip keeps, what finishing writes, where the tab goes next) is plain C++ and
// is tested on its own (first_run_apply_unittest.cc); first_run_handler.cc connects it to
// the profile.

#ifndef COMPONENTS_ATLAS_PAGES_SETUP_FIRST_RUN_APPLY_H_
#define COMPONENTS_ATLAS_PAGES_SETUP_FIRST_RUN_APPLY_H_

namespace atlas_pages::setup {

// The person's choices, as the page sends them (README.md, "The host contract").
struct Choices {
  enum class Start { kClean, kImport };

  bool skipped = false;
  bool do_not_track = true;
  bool global_privacy_control = true;
  bool tracker_blocking = true;
  Start start = Start::kClean;
};

// The places a choice is written to.
class Environment {
 public:
  virtual ~Environment() = default;

  virtual void SetDoNotTrack(bool on) = 0;
  virtual void SetGlobalPrivacyControl(bool on) = 0;
  virtual void SetTrackerBlocking(bool on) = 0;
  // Records that the first run is over, so it does not open again.
  virtual void MarkCompleted() = 0;
};

// Where the tab goes when the page is done.
enum class NextPage { kNewTab, kImportData };

// Applies the choices and returns the page to open in place of the first-run page.
//
// The three privacy choices are written as the page shows them, skipped or not: the page's own
// words on a skip are "keeps the privacy settings you chose". A skip imports nothing and
// leaves the default browser alone; it always ends on the new tab page. Setting the default
// browser is not done here: the page asks for it itself, through the same message the settings
// page uses, before it hands over (host_webui.js).
// Completion is recorded last, after the choices, so a crash in between shows the page again
// instead of losing a choice.
NextPage Apply(const Choices& choices, Environment& env);

}  // namespace atlas_pages::setup

#endif  // COMPONENTS_ATLAS_PAGES_SETUP_FIRST_RUN_APPLY_H_
