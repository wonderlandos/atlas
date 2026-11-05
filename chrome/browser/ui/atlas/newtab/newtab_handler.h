// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_NEWTAB_NEWTAB_HANDLER_H_
#define CHROME_BROWSER_UI_ATLAS_NEWTAB_NEWTAB_HANDLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/atlas_pages/newtab/newtab_state.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace atlas::newtab {

// Answers the new tab page (components/atlas_pages/newtab/newtab.js through
// host.engine.js). Messages from the page, each with a call id first when it
// expects an answer:
//   getState(id)            -> state (newtab_state.h BuildState + strings)
//   open(url, newTab)       web addresses only
//   submit(text)            an address or a search with the default provider
//   pin(id, url, title)     -> null
//   unpin(id, url)          -> null
// An answer runs atlasNewTabHost.resolve(id, value) in the page.
class NewTabHandler : public content::WebUIMessageHandler,
                      public sessions::TabRestoreServiceObserver {
 public:
  explicit NewTabHandler(Profile* profile);
  NewTabHandler(const NewTabHandler&) = delete;
  NewTabHandler& operator=(const NewTabHandler&) = delete;
  ~NewTabHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // sessions::TabRestoreServiceObserver:
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override {}
  void TabRestoreServiceDestroyed(sessions::TabRestoreService* service) override;
  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override;

  // The pages to pick up: the tabs closed most recently first, as the tab
  // restore list keeps them. Public for the tests.
  static std::vector<RecentPage> RecentFrom(
      const sessions::TabRestoreService::Entries& entries);

 private:
  void HandleGetState(const base::ListValue& args);
  void HandleOpen(const base::ListValue& args);
  void HandleSubmit(const base::ListValue& args);
  void HandlePin(const base::ListValue& args);
  void HandleUnpin(const base::ListValue& args);

  void Reply(int call_id);
  void ResolveNull(int call_id);
  base::DictValue Strings() const;
  bool FirstInWindow();
  void OpenWebUrl(const GURL& url, WindowOpenDisposition disposition);

  const raw_ptr<Profile> profile_;
  std::vector<int> waiting_for_restore_list_;
  base::ScopedObservation<sessions::TabRestoreService,
                          sessions::TabRestoreServiceObserver>
      restore_observation_{this};
};

}  // namespace atlas::newtab

#endif  // CHROME_BROWSER_UI_ATLAS_NEWTAB_NEWTAB_HANDLER_H_
