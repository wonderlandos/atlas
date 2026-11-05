// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_chrome.h"

#include <memory>

#include "base/command_line.h"
#include "chrome/browser/ui/atlas/kit/kit_privacy_popover.h"
#include "chrome/browser/ui/atlas/kit/kit_sidebar_view.h"
#include "chrome/browser/ui/atlas/kit/kit_toolbar_view.h"
#include "chrome/browser/ui/atlas/toolbar/toolbar_buttons.h"
#include "chrome/browser/ui/atlas/toolbar/toolbar_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace atlas::kit {

namespace {

KitToolbarView* FindKitToolbar(const views::View* toolbar) {
  for (views::View* child : toolbar->children()) {
    if (auto* kit = views::AsViewClass<KitToolbarView>(child)) {
      return kit;
    }
  }
  return nullptr;
}

}  // namespace

bool IsKitChromeEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kKitChromeSwitch);
}

void InstallKitToolbar(views::View* toolbar,
                       BrowserView* browser_view,
                       views::View* omnibox,
                       const std::vector<views::View*>& actions) {
  if (FindKitToolbar(toolbar)) {
    return;
  }
  InstallKitPrivacyPresenter();
  auto* kit = toolbar->AddChildView(
      std::make_unique<KitToolbarView>(browser_view->browser()));
  kit->SetProperty(views::kViewIgnoredByLayoutKey, true);

  // The Atlas toolbar area's buttons first (privacy, bookmark), then the
  // engine's (downloads among the pinned actions, extensions, menu).
  std::vector<views::View*> all;
  for (views::View* child : toolbar->children()) {
    if (child->GetProperty(views::kElementIdentifierKey) ==
        atlas::toolbar::kPrivacyButtonElementId) {
      all.push_back(child);
    }
  }
  if (views::View* bookmark =
          atlas::toolbar::FindVisibleBookmarkButton(toolbar)) {
    all.push_back(bookmark);
  }
  all.insert(all.end(), actions.begin(), actions.end());
  kit->Adopt(omnibox, all);

  // What is left of the engine's toolbar is replaced by the Kit toolbar.
  const std::vector<views::View*> rest(toolbar->children().begin(),
                                       toolbar->children().end());
  for (views::View* child : rest) {
    if (child != kit) {
      child->SetVisible(false);
    }
  }
}

bool LayoutKitToolbar(views::View* toolbar) {
  KitToolbarView* kit = FindKitToolbar(toolbar);
  if (!kit) {
    return false;
  }
  kit->SetBoundsRect(toolbar->GetLocalBounds());
  return true;
}

std::optional<gfx::Size> KitToolbarSize(const views::View* toolbar) {
  if (KitToolbarView* kit = FindKitToolbar(toolbar)) {
    return kit->GetPreferredSize();
  }
  return std::nullopt;
}

void InstallKitSidebar(views::View* vertical_tab_strip_region,
                       BrowserWindowInterface* browser) {
  views::View* content_area = nullptr;
  for (views::View* child : vertical_tab_strip_region->children()) {
    if (views::IsViewClass<views::FlexLayoutView>(child)) {
      content_area = child;
      break;
    }
  }
  if (!content_area) {
    return;
  }
  auto sidebar = std::make_unique<KitSidebarView>(browser);
  sidebar->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  KitSidebarView* view = content_area->AddChildViewAt(std::move(sidebar), 0);
  view->TakeOver(content_area);
}

}  // namespace atlas::kit
