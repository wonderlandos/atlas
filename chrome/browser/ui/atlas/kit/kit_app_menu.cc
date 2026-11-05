// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_app_menu.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "chrome/browser/ui/atlas/kit/kit_chrome.h"
#include "chrome/browser/ui/atlas/kit/kit_menu.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view_tracker.h"

namespace atlas::kit {

namespace {

std::u16string ZoomText(BrowserWindowInterface* browser) {
  tabs::TabInterface* tab = browser->GetActiveTabInterface();
  content::WebContents* contents = tab ? tab->GetContents() : nullptr;
  zoom::ZoomController* zoom =
      contents ? zoom::ZoomController::FromWebContents(contents) : nullptr;
  return base::FormatPercent(zoom ? zoom->GetZoomPercent() : 100);
}

}  // namespace

bool ShowKitAppMenu(views::View* anchor,
                    BrowserWindowInterface* browser,
                    ui::AcceleratorProvider* provider,
                    AppMenuIconController* icon_controller) {
  if (!IsKitChromeEnabled()) {
    return false;
  }
  auto model = std::make_shared<AppMenuModel>(
      provider, browser, icon_controller,
      AppMenuModel::GetAlertItemForRunningTutorial(browser));
  model->Init();
  MenuAnchor menu_anchor;
  menu_anchor.view = std::make_shared<views::ViewTracker>(anchor);
  menu_anchor.arrow = views::BubbleBorder::TOP_RIGHT;
  menu_anchor.zoom_text = base::BindRepeating(&ZoomText, browser);
  ShowMenuModel(std::move(menu_anchor), model->AsWeakPtr(), model);
  return true;
}

}  // namespace atlas::kit
