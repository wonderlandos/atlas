// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/toolbar/bookmark_button.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/atlas/toolbar/toolbar_prefs.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

namespace atlas::toolbar {

BookmarkButton::BookmarkButton(BrowserWindowInterface* browser)
    : ToolbarButton(base::BindRepeating(
          [](BrowserWindowInterface* browser, const ui::Event&) {
            chrome::BookmarkCurrentTab(browser);
          },
          base::Unretained(browser))),
      browser_(browser) {
  SetProperty(views::kElementIdentifierKey, kBookmarkStarViewElementId);
  active_tab_subscription_ = browser_->RegisterActiveTabDidChange(
      base::BindRepeating([](BookmarkButton* button, BrowserWindowInterface*) {
        button->ActiveTabChanged();
      }, base::Unretained(this)));
  SetStarred(false);
  ActiveTabChanged();

  pref_registrar_.Init(browser_->GetProfile()->GetPrefs());
  pref_registrar_.Add(kShowBookmarkButton,
                      base::BindRepeating(&BookmarkButton::UpdateVisibility,
                                          base::Unretained(this)));
  UpdateVisibility();
}

BookmarkButton::~BookmarkButton() = default;

SkColor BookmarkButton::GetForegroundColor(ButtonState state) const {
  const ui::ColorProvider* color_provider = GetColorProvider();
  if (!color_provider || !starred_) {
    return ToolbarButton::GetForegroundColor(state);
  }
  return color_provider->GetColor(ui::kColorAccent);
}

void BookmarkButton::URLStarredChanged(content::WebContents* web_contents,
                                       bool starred) {
  SetStarred(starred);
}

void BookmarkButton::WebContentsDestroyed() {
  tab_helper_observation_.Reset();
  Observe(nullptr);
}

void BookmarkButton::ActiveTabChanged() {
  tab_helper_observation_.Reset();
  tabs::TabInterface* tab = browser_->GetActiveTabInterface();
  content::WebContents* contents = tab ? tab->GetContents() : nullptr;
  Observe(contents);
  BookmarkTabHelper* helper =
      contents ? BookmarkTabHelper::FromWebContents(contents) : nullptr;
  if (helper) {
    tab_helper_observation_.Observe(helper);
  }
  SetStarred(helper && helper->is_starred());
}

void BookmarkButton::SetStarred(bool starred) {
  starred_ = starred;
  const std::u16string text =
      l10n_util::GetStringUTF16(starred ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR);
  SetTooltipText(text);
  GetViewAccessibility().SetName(text);
  SetVectorIcon(starred ? omnibox::kStarFilledIcon : omnibox::kStarIcon);
  if (GetColorProvider()) {
    UpdateIcon();
  }
}

void BookmarkButton::UpdateVisibility() {
  SetVisible(
      browser_->GetProfile()->GetPrefs()->GetBoolean(kShowBookmarkButton));
}

BEGIN_METADATA(BookmarkButton)
END_METADATA

}  // namespace atlas::toolbar
