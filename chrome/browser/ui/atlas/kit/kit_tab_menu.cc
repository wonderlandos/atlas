// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_tab_menu.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/atlas/kit/kit_menu.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_interface.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "content/public/browser/context_menu_params.h"
#endif

namespace atlas::kit {

namespace {

// The delegate of the engine's TabMenuModel (what TabContextMenuController is
// for the engine's tab strip): commands are TabStripModel context-menu commands
// for the tab, extension items go to the model's matcher.
class TabMenu : public ui::SimpleMenuModel::Delegate {
 public:
  TabMenu(BrowserWindowInterface* browser,
          int index,
          views::Widget* widget)
      : browser_(browser),
        model_(browser->GetTabStripModel()),
        tab_(model_->GetTabAtIndex(index)->GetHandle()),
        widget_(widget->GetWeakPtr()) {
    menu_ = std::make_unique<TabMenuModel>(
        this, TabMenuModelDelegate::From(browser), model_, index);
  }
  ~TabMenu() override = default;

  TabMenuModel* menu() { return menu_.get(); }

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
            command_id)) {
      return menu_->extension_items() &&
             menu_->extension_items()->IsCommandIdChecked(command_id);
    }
#endif
    return false;
  }
  bool IsCommandIdEnabled(int command_id) const override {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
            command_id)) {
      return menu_->extension_items() &&
             menu_->extension_items()->IsCommandIdEnabled(command_id);
    }
#endif
    tabs::TabInterface* tab = tab_.Get();
    // The tab may have left this window while the menu was open (an extension
    // moved it, a drag in another window): its index is kNoTab then, and the
    // model CHECKs an index (QA-A9-WEB-ATLAS-11).
    const int index = tab ? model_->GetIndexOfTab(tab) : TabStripModel::kNoTab;
    return index != TabStripModel::kNoTab &&
           model_->IsContextMenuCommandEnabled(
               index, static_cast<TabStripModel::ContextMenuCommand>(command_id));
  }
  bool IsCommandIdVisible(int command_id) const override {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
            command_id)) {
      return menu_->extension_items() &&
             menu_->extension_items()->IsCommandIdVisible(command_id);
    }
#endif
    return true;
  }
  void ExecuteCommand(int command_id, int event_flags) override {
    tabs::TabInterface* tab = tab_.Get();
    if (!tab) {
      return;
    }
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
            command_id)) {
      if (menu_->extension_items() && tab->GetContents()) {
        content::ContextMenuParams params;
        params.page_url = tab->GetContents()->GetLastCommittedURL();
        menu_->extension_items()->ExecuteCommand(command_id, tab->GetContents(),
                                                 nullptr, params);
      }
      return;
    }
#endif
    const int index = model_->GetIndexOfTab(tab);
    if (index == TabStripModel::kNoTab) {
      return;  // the tab left this window while the menu was open
    }
    model_->ExecuteContextMenuCommand(
        index, static_cast<TabStripModel::ContextMenuCommand>(command_id));
  }
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override {
    int browser_command;
    return widget_ &&
           TabStripModel::ContextMenuCommandToBrowserCommand(command_id,
                                                             &browser_command) &&
           widget_->GetAccelerator(browser_command, accelerator);
  }

 private:
  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<TabStripModel> model_;
  tabs::TabHandle tab_;
  base::WeakPtr<views::Widget> widget_;
  std::unique_ptr<TabMenuModel> menu_;
};

}  // namespace

void ShowKitTabMenu(BrowserWindowInterface* browser,
                    int tab_index,
                    views::Widget* widget,
                    const gfx::Rect& screen_rect) {
  TabStripModel* model = browser->GetTabStripModel();
  if (!widget || tab_index < 0 || tab_index >= model->count()) {
    return;
  }
  auto session = std::make_shared<TabMenu>(browser, tab_index, widget);
  ShowKitContextMenu(widget, screen_rect, session->menu()->AsWeakPtr(),
                     session);
}

}  // namespace atlas::kit
