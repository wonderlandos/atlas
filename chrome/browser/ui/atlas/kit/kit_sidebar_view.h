// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_SIDEBAR_VIEW_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_SIDEBAR_VIEW_H_

#include <string>
#include <vector>

#include "base/callback_list.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace views {
class ScrollView;
}

namespace gfx {
class Point;
}

namespace atlas::kit {

// The Kit sidebar (tokens.md section 3; reel scenes R02 and R09): the tab
// list as sections. PINNED, then the tabs by the day they were last used
// (TODAY, YESTERDAY, EARLIER), a named group under its own header (a collapsed
// group is one "N tabs" row), and the footer row "New tab" with its shortcut.
// Rows are 28 px with an 8 px inset and radius 8; the tab in front is the
// accent pill. It reads the engine's tab model and drives it (activate, new
// tab); it keeps no tab state of its own. Below 120 px wide the rows show
// their glyph alone (the collapsed sidebar).
class KitSidebarView : public views::View,
                       public TabStripModelObserver,
                       public views::ViewObserver {
  METADATA_HEADER(KitSidebarView, views::View)

 public:
  explicit KitSidebarView(BrowserWindowInterface* browser);
  KitSidebarView(const KitSidebarView&) = delete;
  KitSidebarView& operator=(const KitSidebarView&) = delete;
  ~KitSidebarView() override;

  // Hides the other children of `content_area` now and whenever the engine
  // adds one later (its tab list, its new-tab button).
  void TakeOver(views::View* content_area);

  // views::View:
  // F6 (pane focus) lands on the row of the tab in front, else the first row.
  void RequestFocus() override;
  void Layout(PassKey) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      TabChangeType change_type) override;
  void OnTabPinnedStateChanged(tabs::TabInterface* tab, int index) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;

  // views::ViewObserver:
  void OnChildViewAdded(views::View* observed_view,
                        views::View* child) override;
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  // Builds the rows again from the tab model: when the SHAPE of the list
  // changed (a tab added, removed, moved, pinned, grouped). A change of one
  // tab's title, icon, loading or sound updates its row in place
  // (RefreshRow); rebuilding for that deleted the focused row, the row being
  // dragged and the row's menu many times a second during a load.
  void Rebuild();
  // Updates the row of `contents` (title, icon, rest, sound) without touching
  // the other rows; nothing when the tab has no row (inside a folded group).
  void RefreshRow(content::WebContents* contents);
  // Brings the row of the tab in front into the visible part of the list.
  void ScrollActiveRowToVisible();
  void ToggleMute(content::WebContents* contents);
  void AddHeader(const std::u16string& text);

  // What a tab row asks of the sidebar: close its tab, show its menu, reorder
  // by dragging. All of them are posted or non-destructive: the row that asks
  // is deleted by the rebuild that follows.
  void ToggleGroup(tab_groups::TabGroupId id);
  void EditGroup(tab_groups::TabGroupId id);
  void CloseTab(content::WebContents* contents);
  void ShowTabMenu(content::WebContents* contents, const gfx::Point& screen);
  void DragTab(content::WebContents* contents,
               views::View* row,
               bool end,
               const gfx::Point& point_in_row);
  void MoveTab(int from, int to);
  void ClearDrag();

  const raw_ptr<BrowserWindowInterface> browser_;
  // The rows scroll when they are more than fit above the footer.
  raw_ptr<views::ScrollView> scroll_ = nullptr;
  raw_ptr<views::View> list_ = nullptr;
  raw_ptr<views::View> footer_ = nullptr;
  // The line that shows where a dragged tab will land, and the tab rows (view
  // and model index) the drag measures against.
  struct TabRow {
    raw_ptr<views::View> view;
    int index;
    base::WeakPtr<content::WebContents> contents;
  };
  std::vector<TabRow> tab_rows_;
  // One per tab row: the tab's alert state (sound) changed.
  std::vector<base::CallbackListSubscription> alert_subscriptions_;
  raw_ptr<views::View> drop_line_ = nullptr;
  raw_ptr<views::View> dragged_row_ = nullptr;
  int drop_before_ = -1;
  base::WeakPtrFactory<KitSidebarView> weak_factory_{this};
  base::ScopedObservation<views::View, views::ViewObserver> content_area_{
      this};
};

}  // namespace atlas::kit

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_SIDEBAR_VIEW_H_
