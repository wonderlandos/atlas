// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/newtab/newtab_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/atlas/newtab/newtab_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "components/atlas_pages/common/atlas_page_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace atlas::newtab {

NewTabUI::NewTabUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  // components/atlas_pages/newtab/: the page, its stylesheets and the
  // engine adapter (served as host.js); theme.css is written from the live
  // theme by the shared page source, which also sets the page's CSP.
  atlas_pages::AtlasPageSource::CreateAndAdd(profile, chrome::kAtlasNewTabHost,
                                             "newtab");

  web_ui->AddMessageHandler(std::make_unique<NewTabHandler>(profile));
}

NewTabUI::~NewTabUI() = default;

NewTabUIConfig::NewTabUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kAtlasNewTabHost) {}

}  // namespace atlas::newtab
