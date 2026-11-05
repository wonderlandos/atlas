// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_CHROME_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_CHROME_H_

#include <optional>
#include <vector>

#include "ui/gfx/geometry/size.h"

class BrowserWindowInterface;
class BrowserView;

namespace views {
class View;
}  // namespace views

// The Kit chrome (ADR 0062): Atlas draws its own toolbar and sidebar as views
// of this directory, with the WolOS Kit numbers, inside the engine's window.
// The engine keeps the page, the omnibox's logic, extensions, downloads and
// the tab model; the hooks below are the only places engine code calls in.
namespace atlas::kit {

// The command-line switch that turns the Kit chrome on (phase 1: off by
// default, so the patch-based chrome stays the shipped look).
inline constexpr char kKitChromeSwitch[] = "atlas-kit-chrome";

// Whether this browser process draws the Kit chrome.
bool IsKitChromeEnabled();

// Hook in ToolbarView::Init() (normal display mode only): adds the Kit toolbar
// to `toolbar`, moves `omnibox` and the `actions` (left to right; null entries
// are skipped) into it, and hides the stock children that the Kit toolbar
// replaces. The privacy and bookmark buttons of the Atlas toolbar area are
// found among the children and moved in first.
void InstallKitToolbar(views::View* toolbar,
                       BrowserView* browser_view,
                       views::View* omnibox,
                       const std::vector<views::View*>& actions);

// Hook at the top of ToolbarView::Layout(): lays the Kit toolbar over the
// whole toolbar and returns true when `toolbar` has one.
bool LayoutKitToolbar(views::View* toolbar);

// Hook at the top of ToolbarView::CalculatePreferredSize() and
// GetMinimumSize(): the Kit toolbar's size when `toolbar` has one.
std::optional<gfx::Size> KitToolbarSize(const views::View* toolbar);

// Hook in the BrowserView constructor, after the vertical tab strip region is
// made: puts the Kit sidebar in the region's content area in place of the
// engine's tab list and new-tab button.
void InstallKitSidebar(views::View* vertical_tab_strip_region,
                       BrowserWindowInterface* browser);

}  // namespace atlas::kit

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_CHROME_H_
