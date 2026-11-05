// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 The Atlas Authors.

#ifndef COMPONENTS_ATLAS_PAGES_SETUP_FIRST_RUN_HANDLER_H_
#define COMPONENTS_ATLAS_PAGES_SETUP_FIRST_RUN_HANDLER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/atlas_pages/setup/first_run_apply.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "url/gurl.h"

class PrefService;

namespace content {
class BrowserContext;
}

namespace atlas_pages::setup {

// The two messages of the page (host_webui.js):
//   "atlasFirstRun.getState" [callbackId]  -> { privacy: { doNotTrack, globalPrivacyControl,
//                                               trackerBlocking }, reducedMotion: null }
//   "atlasFirstRun.finish"   [choices]     -> applies them and replaces the tab.
// Whether Atlas is the default browser is answered by the settings page's own handler,
// which the same page also carries.
inline constexpr char kGetStateMessage[] = "atlasFirstRun.getState";
inline constexpr char kFinishMessage[] = "atlasFirstRun.finish";

// Reads the choices the page sends. Returns nullopt for anything that is not exactly what the
// page sends (wrong type, missing field, unknown start value): nothing is applied then.
std::optional<Choices> ParseChoices(const base::DictValue& dict);

// The state the page starts from.
base::DictValue BuildState(const PrefService& prefs);

// The page opened in place of the first-run page.
GURL NextPageUrl(NextPage page);

// Writes the choices into the profile's preferences. The blocker also has a live effect (the
// built-in component is unloaded at once), which is a callback because it is not a preference.
class ProfileEnvironment : public Environment {
 public:
  using BlockerSwitch = base::RepeatingCallback<void(bool)>;

  ProfileEnvironment(PrefService* prefs, BlockerSwitch switch_blocker);
  ~ProfileEnvironment() override;

  void SetDoNotTrack(bool on) override;
  void SetGlobalPrivacyControl(bool on) override;
  void SetTrackerBlocking(bool on) override;
  void MarkCompleted() override;

 private:
  raw_ptr<PrefService> prefs_;
  BlockerSwitch switch_blocker_;
};

class AtlasFirstRunHandler : public content::WebUIMessageHandler {
 public:
  // `switch_blocker` is what makes a change of the blocker take effect now (the built-in
  // component is unloaded); the host of the page provides it.
  AtlasFirstRunHandler(content::BrowserContext* context,
                       ProfileEnvironment::BlockerSwitch switch_blocker);
  AtlasFirstRunHandler(const AtlasFirstRunHandler&) = delete;
  AtlasFirstRunHandler& operator=(const AtlasFirstRunHandler&) = delete;
  ~AtlasFirstRunHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleGetState(const base::ListValue& args);
  void HandleFinish(const base::ListValue& args);

  raw_ptr<content::BrowserContext> context_;
  ProfileEnvironment::BlockerSwitch switch_blocker_;
};

}  // namespace atlas_pages::setup

#endif  // COMPONENTS_ATLAS_PAGES_SETUP_FIRST_RUN_HANDLER_H_
