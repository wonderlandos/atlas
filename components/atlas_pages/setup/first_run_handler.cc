// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 The Atlas Authors.

#include "components/atlas_pages/setup/first_run_handler.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/atlas_pages/setup/setup_prefs.h"
#include "components/helium_services/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/page_transition_types.h"

namespace atlas_pages::setup {

namespace {

// The settings page that imports from another browser (the route is kept by the settings
// patches; the real-window walk of reel-check scene R04 opens it).
constexpr char kImportDataUrl[] = "chrome://settings/importData";

}  // namespace

std::optional<Choices> ParseChoices(const base::DictValue& dict) {
  Choices choices;
  std::optional<bool> skipped = dict.FindBool("skipped");
  if (!skipped) {
    return std::nullopt;
  }
  choices.skipped = *skipped;

  const base::DictValue* privacy = dict.FindDict("privacy");
  if (!privacy) {
    return std::nullopt;
  }
  std::optional<bool> dnt = privacy->FindBool("doNotTrack");
  std::optional<bool> gpc = privacy->FindBool("globalPrivacyControl");
  std::optional<bool> blocking = privacy->FindBool("trackerBlocking");
  if (!dnt || !gpc || !blocking) {
    return std::nullopt;
  }
  choices.do_not_track = *dnt;
  choices.global_privacy_control = *gpc;
  choices.tracker_blocking = *blocking;

  if (choices.skipped) {
    // A skip imports nothing, whatever "start" says.
    return choices;
  }

  const std::string* start = dict.FindString("start");
  if (!start) {
    return std::nullopt;
  }
  if (*start == "import") {
    choices.start = Choices::Start::kImport;
  } else if (*start == "clean") {
    choices.start = Choices::Start::kClean;
  } else {
    return std::nullopt;
  }
  return choices;
}

base::DictValue BuildState(const PrefService& prefs) {
  return base::DictValue()
      .Set("reducedMotion", base::Value())
      .Set("privacy",
           base::DictValue()
               .Set("doNotTrack", prefs.GetBoolean(prefs::kEnableDoNotTrack))
               .Set("globalPrivacyControl",
                    prefs.GetBoolean(prefs::kHeliumGlobalPrivacyControl))
               .Set("trackerBlocking", prefs.GetBoolean(kTrackerBlocking)));
}

GURL NextPageUrl(NextPage page) {
  return GURL(page == NextPage::kImportData ? kImportDataUrl
                                            : chrome::kChromeUINewTabURL);
}

ProfileEnvironment::ProfileEnvironment(PrefService* prefs,
                                       BlockerSwitch switch_blocker)
    : prefs_(prefs), switch_blocker_(std::move(switch_blocker)) {}

ProfileEnvironment::~ProfileEnvironment() = default;

void ProfileEnvironment::SetDoNotTrack(bool on) {
  prefs_->SetBoolean(prefs::kEnableDoNotTrack, on);
}

void ProfileEnvironment::SetGlobalPrivacyControl(bool on) {
  prefs_->SetBoolean(prefs::kHeliumGlobalPrivacyControl, on);
}

void ProfileEnvironment::SetTrackerBlocking(bool on) {
  prefs_->SetBoolean(kTrackerBlocking, on);
  switch_blocker_.Run(on);
}

void ProfileEnvironment::MarkCompleted() {
  prefs_->SetBoolean(prefs::kHeliumDidOnboarding, true);
}

AtlasFirstRunHandler::AtlasFirstRunHandler(
    content::BrowserContext* context,
    ProfileEnvironment::BlockerSwitch switch_blocker)
    : context_(context), switch_blocker_(std::move(switch_blocker)) {}

AtlasFirstRunHandler::~AtlasFirstRunHandler() = default;

void AtlasFirstRunHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kGetStateMessage,
      base::BindRepeating(&AtlasFirstRunHandler::HandleGetState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kFinishMessage,
      base::BindRepeating(&AtlasFirstRunHandler::HandleFinish,
                          base::Unretained(this)));
}

void AtlasFirstRunHandler::HandleGetState(const base::ListValue& args) {
  if (args.size() != 1 || !args[0].is_string()) {
    return;
  }
  AllowJavascript();
  ResolveJavascriptCallback(
      args[0],
      base::Value(BuildState(*user_prefs::UserPrefs::Get(context_))));
}

void AtlasFirstRunHandler::HandleFinish(const base::ListValue& args) {
  if (args.size() != 1 || !args[0].is_dict()) {
    return;
  }
  std::optional<Choices> choices = ParseChoices(args[0].GetDict());
  if (!choices) {
    return;
  }

  ProfileEnvironment env(user_prefs::UserPrefs::Get(context_), switch_blocker_);
  const NextPage next = Apply(*choices, env);

  // Last: replacing the page ends this handler's life.
  content::NavigationController::LoadURLParams params(NextPageUrl(next));
  params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  params.should_replace_current_entry = true;
  web_ui()->GetWebContents()->GetController().LoadURLWithParams(params);
}

}  // namespace atlas_pages::setup
