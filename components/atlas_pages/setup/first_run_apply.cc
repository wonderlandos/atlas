// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 The Atlas Authors.

#include "components/atlas_pages/setup/first_run_apply.h"

namespace atlas_pages::setup {

NextPage Apply(const Choices& choices, Environment& env) {
  env.SetDoNotTrack(choices.do_not_track);
  env.SetGlobalPrivacyControl(choices.global_privacy_control);
  env.SetTrackerBlocking(choices.tracker_blocking);
  env.MarkCompleted();
  if (choices.skipped) {
    return NextPage::kNewTab;
  }
  return choices.start == Choices::Start::kImport ? NextPage::kImportData
                                                  : NextPage::kNewTab;
}

}  // namespace atlas_pages::setup
