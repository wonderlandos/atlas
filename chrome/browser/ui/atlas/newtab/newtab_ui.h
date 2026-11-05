// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_NEWTAB_NEWTAB_UI_H_
#define CHROME_BROWSER_UI_ATLAS_NEWTAB_NEWTAB_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace atlas::newtab {

// The new tab page of Atlas, served at the engine's own new tab address
// (host "new-tab-page"), so everything that already knows that address (the
// tab title and icon, the empty address bar, the permissions) keeps working.
// Incognito and guest windows keep the engine's page.
class NewTabUI : public content::WebUIController {
 public:
  explicit NewTabUI(content::WebUI* web_ui);
  NewTabUI(const NewTabUI&) = delete;
  NewTabUI& operator=(const NewTabUI&) = delete;
  ~NewTabUI() override;
};

class NewTabUIConfig : public content::DefaultWebUIConfig<NewTabUI> {
 public:
  NewTabUIConfig();
};

}  // namespace atlas::newtab

#endif  // CHROME_BROWSER_UI_ATLAS_NEWTAB_NEWTAB_UI_H_
