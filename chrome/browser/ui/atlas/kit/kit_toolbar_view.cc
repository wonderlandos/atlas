// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_toolbar_view.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"
#include "chrome/browser/ui/atlas/kit/kit_icon_button.h"
#include "chrome/browser/ui/atlas/kit/kit_layout.h"
#include "chrome/browser/ui/atlas/kit/kit_menu.h"
#include "chrome/browser/ui/atlas/kit/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/vector_icons.h"

namespace atlas::kit {

KitToolbarView::KitToolbarView(BrowserWindowInterface* browser) : browser_(browser) {
  // The reel's order (R02): sidebar toggle, back, forward, reload.
  sidebar_ = AddChildView(std::make_unique<KitIconButton>(
      base::BindRepeating(&KitToolbarView::ToggleSidebar,
                          base::Unretained(this)),
      views::kSidebarLeftIcon,
      l10n_util::GetStringUTF16(IDS_COLLAPSE_VERTICAL_TABS)));
  back_ = AddChildView(std::make_unique<KitIconButton>(
      base::BindRepeating(&KitToolbarView::Run, base::Unretained(this),
                          IDC_BACK),
      kKitGoPreviousIcon,
      l10n_util::GetStringUTF16(IDS_TOOLTIP_BACK)));
  forward_ = AddChildView(std::make_unique<KitIconButton>(
      base::BindRepeating(&KitToolbarView::Run, base::Unretained(this),
                          IDC_FORWARD),
      kKitGoNextIcon,
      l10n_util::GetStringUTF16(IDS_TOOLTIP_FORWARD)));
  reload_ = AddChildView(std::make_unique<KitIconButton>(
      base::BindRepeating(&KitToolbarView::ReloadOrStop,
                          base::Unretained(this)),
      kKitViewRefreshIcon,
      l10n_util::GetStringUTF16(IDS_TOOLTIP_RELOAD)));

  back_->set_context_menu_controller(this);
  forward_->set_context_menu_controller(this);

  for (int id : {IDC_BACK, IDC_FORWARD, IDC_RELOAD, IDC_STOP}) {
    chrome::AddCommandObserver(browser_, id, this);
    EnabledStateChangedForCommand(id, chrome::IsCommandEnabled(browser_, id));
  }
}

KitToolbarView::~KitToolbarView() {
  for (int id : {IDC_BACK, IDC_FORWARD, IDC_RELOAD, IDC_STOP}) {
    chrome::RemoveCommandObserver(browser_, id, this);
  }
}

void KitToolbarView::Adopt(views::View* omnibox,
                           const std::vector<views::View*>& actions) {
  if (omnibox) {
    omnibox_ = AddChildView(omnibox->parent()->RemoveChildViewT(omnibox));
  }
  for (views::View* action : actions) {
    if (!action) {
      continue;
    }
    actions_.push_back(AddChildView(action->parent()->RemoveChildViewT(action)));
  }
  InvalidateLayout();
}

gfx::Size KitToolbarView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(0, tokens::kToolbarHeight);
}

gfx::Size KitToolbarView::GetMinimumSize() const {
  return gfx::Size(0, tokens::kToolbarHeight);
}

void KitToolbarView::Layout(PassKey) {
  std::vector<std::optional<int>> widths;
  for (views::View* action : actions_) {
    widths.push_back(action->GetVisible()
                         ? std::optional<int>(action->GetPreferredSize().width())
                         : std::nullopt);
  }
  const ToolbarGeometry g = LayoutToolbar(size(), widths);
  sidebar_->SetBoundsRect(g.buttons[0]);
  back_->SetBoundsRect(g.buttons[1]);
  forward_->SetBoundsRect(g.buttons[2]);
  reload_->SetBoundsRect(g.buttons[3]);
  for (size_t i = 0; i < actions_.size(); ++i) {
    if (widths[i]) {
      actions_[i]->SetBoundsRect(g.actions[i]);
    }
  }
  if (omnibox_) {
    omnibox_->SetBoundsRect(g.omnibox);
  }
}

void KitToolbarView::OnPaintBackground(gfx::Canvas* canvas) {
  const ui::ColorProvider* provider = GetColorProvider();
  if (!provider) {
    return;
  }
  canvas->DrawColor(provider->GetColor(kColorToolbar));
  // No line of our own under the bar: the content area draws the 1 px `line`
  // along its top and left edge, and the sidebar draws the part of it that is
  // above itself, at the same row (a second line here made it 2 px thick:
  // E2E-11).
}

void KitToolbarView::EnabledStateChangedForCommand(int id, bool enabled) {
  switch (id) {
    case IDC_BACK:
      back_->SetEnabled(enabled);
      break;
    case IDC_FORWARD:
      forward_->SetEnabled(enabled);
      break;
    case IDC_RELOAD:
      reload_->SetEnabled(enabled || loading_);
      break;
    case IDC_STOP:
      // The engine enables stop while a page loads: reload becomes stop.
      loading_ = enabled;
      if (loading_) {
        reload_->SetIcon(kKitProcessStopIcon);
        reload_->SetTooltip(l10n_util::GetStringUTF16(IDS_TOOLTIP_STOP));
        reload_->SetEnabled(true);
      } else {
        reload_->SetIcon(kKitViewRefreshIcon);
        reload_->SetTooltip(l10n_util::GetStringUTF16(IDS_TOOLTIP_RELOAD));
        reload_->SetEnabled(chrome::IsCommandEnabled(browser_, IDC_RELOAD));
      }
      break;
  }
}

void KitToolbarView::ReloadOrStop() {
  chrome::ExecuteCommand(browser_, loading_ ? IDC_STOP : IDC_RELOAD);
}

void KitToolbarView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (source != back_ && source != forward_) {
    return;
  }
  auto model = std::make_shared<BackForwardMenuModel>(
      browser_, source == back_ ? BackForwardMenuModel::ModelType::kBackward
                                : BackForwardMenuModel::ModelType::kForward);
  if (model->GetItemCount() == 0) {
    return;
  }
  ShowKitContextMenu(GetWidget(), gfx::Rect(point, gfx::Size()),
                     model->AsWeakPtr(), model);
}

void KitToolbarView::Run(int command_id) {
  chrome::ExecuteCommand(browser_, command_id);
}

void KitToolbarView::ToggleSidebar() {
  auto* controller = tabs::VerticalTabStripStateController::From(browser_);
  if (controller) {
    controller->RequestCollapse(!controller->IsCollapsed());
  }
}

BEGIN_METADATA(KitToolbarView)
END_METADATA

}  // namespace atlas::kit
