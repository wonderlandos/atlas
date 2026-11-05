// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/brand/atlas_about.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ui/atlas/brand/wolos_release.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

namespace atlas::brand {

namespace {

void RunSoftwareUpdate() {
  base::LaunchOptions options;
  base::Process process = base::LaunchProcess(
      std::vector<std::string>{"wol-settings", "Software Update"}, options);
  if (process.IsValid()) {
    base::EnsureProcessGetsReaped(std::move(process));
  }
}

}  // namespace

std::u16string AnnotatedVersion(std::string_view atlas_version,
                                std::string_view engine_version,
                                const std::u16string& channel,
                                const std::u16string& bits) {
  const std::string& wolos = GetWolosRelease();
  if (wolos.empty()) {
    return l10n_util::GetStringFUTF16(IDS_ATLAS_ABOUT_VERSION,
                                      base::UTF8ToUTF16(atlas_version), channel,
                                      bits, base::UTF8ToUTF16(engine_version));
  }
  return l10n_util::GetStringFUTF16(
      IDS_ATLAS_ABOUT_VERSION_WOLOS, base::UTF8ToUTF16(atlas_version), channel,
      bits, base::UTF8ToUTF16(wolos), base::UTF8ToUTF16(engine_version));
}

std::u16string LicenseSentence(const std::u16string& credits_url) {
  return l10n_util::GetStringFUTF16(
      IDS_ATLAS_ABOUT_LICENSE, credits_url,
      l10n_util::GetStringUTF16(IDS_ATLAS_ABOUT_OPENS_IN_NEW_TAB));
}

void AddVersionPageStrings(content::WebUIDataSource* html_source) {
  const std::string& wolos = GetWolosRelease();
  html_source->AddString(
      "atlas_wolos_label",
      l10n_util::GetStringUTF16(IDS_ATLAS_VERSION_WOLOS_LABEL));
  html_source->AddString(
      "atlas_engine_label",
      l10n_util::GetStringUTF16(IDS_ATLAS_VERSION_ENGINE_LABEL));
  html_source->AddString(
      "atlas_wolos_release",
      wolos.empty() ? l10n_util::GetStringUTF16(IDS_ATLAS_VERSION_NO_WOLOS)
                    : base::UTF8ToUTF16(wolos));
}

void OpenSoftwareUpdate() {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&RunSoftwareUpdate));
}

}  // namespace atlas::brand
