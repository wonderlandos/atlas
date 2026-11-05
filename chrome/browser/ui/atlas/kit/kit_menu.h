// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_MENU_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_MENU_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget.h"

namespace views {
class View;
class ViewTracker;
class Widget;
}  // namespace views

namespace atlas::kit {

// Where a Kit menu opens: under `view`, or at `rect` (screen coordinates) in
// the window `parent` when there is no view (a context menu at the pointer).
struct MenuAnchor {
  std::shared_ptr<views::ViewTracker> view;
  gfx::Rect rect;
  gfx::NativeView parent = gfx::NativeView();
  views::BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_RIGHT;
  // The first menu, for the back row of a submenu (set by the Kit).
  base::WeakPtr<ui::MenuModel> root;
  // A bubble that opened this menu (the downloads list): it stays open while
  // the menu has the focus, as the engine's own menu runner leaves it.
  base::WeakPtr<views::Widget> hold_open;
  // The page zoom as text ("100%") for a zoom row; the engine's own menu reads
  // it from the zoom controller, a model does not carry it. Null: no number.
  base::RepeatingCallback<std::u16string()> zoom_text;
};

// Draws the engine's `model` as a Kit popover (rows of 28, radius 12, accent
// row, separators; submenus as a popover with a back row). The model stays
// the engine's: entries, enabled state, checks and shortcuts come from it, and
// running a row calls `ActivatedAt` after the popover closes (nothing happens
// if the model is gone by then). `keepalive` holds whatever owns the model.
void ShowMenuModel(MenuAnchor anchor,
                   base::WeakPtr<ui::MenuModel> model,
                   std::shared_ptr<void> keepalive);

// A context menu at `screen_rect` of `parent`: ShowMenuModel() when the Kit
// chrome is on, and returns true; false (nothing shown) when it is off.
// `keepalive` holds whatever owns `model` until the menu is done.
bool ShowKitContextMenu(views::Widget* parent,
                        const gfx::Rect& screen_rect,
                        base::WeakPtr<ui::MenuModel> model,
                        std::shared_ptr<void> keepalive = nullptr);

}  // namespace atlas::kit

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_MENU_H_
