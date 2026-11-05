// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/toolbar/toolbar_buttons.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/atlas/toolbar/bookmark_button.h"
#include "chrome/browser/ui/atlas/toolbar/privacy_button.h"
#include "chrome/browser/ui/atlas/toolbar/toolbar_prefs.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace atlas::toolbar {

void AddToolbarButtons(views::View* toolbar, BrowserWindowInterface* browser) {
  toolbar->AddChildView(std::make_unique<PrivacyButton>(
      browser->GetProfile()->GetPrefs(),
      base::BindRepeating(
          [](BrowserWindowInterface* browser) {
            chrome::ShowSettingsSubPage(browser, chrome::kPrivacySubPage);
          },
          base::Unretained(browser))));
  toolbar->AddChildView(std::make_unique<BookmarkButton>(browser));
}

views::View* FindVisibleBookmarkButton(views::View* toolbar) {
  for (views::View* child : toolbar->children()) {
    if (child->GetProperty(views::kElementIdentifierKey) ==
            kBookmarkStarViewElementId &&
        child->GetVisible()) {
      return child;
    }
  }
  return nullptr;
}

bool BookmarkButtonReplacesStar(const PrefService* prefs) {
  // A pref service without the pref (a unit test of the engine) keeps the engine's star.
  return prefs->FindPreference(kShowBookmarkButton) &&
         prefs->GetBoolean(kShowBookmarkButton);
}

}  // namespace atlas::toolbar
