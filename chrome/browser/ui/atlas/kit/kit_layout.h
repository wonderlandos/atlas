// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_LAYOUT_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_LAYOUT_H_

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ui/atlas/sidebar/sidebar_sections.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

// The geometry and the list model of the Kit chrome as plain functions, so the
// numbers are tested without a window (atlas_kit_unittests) and the views only
// paint what these return.
namespace atlas::kit {

// Toolbar (tokens.md sections 2 and 4).
inline constexpr int kToolbarEdgeInset = 4;
inline constexpr int kOmniboxGap = 8;

struct ToolbarGeometry {
  // Sidebar toggle, back, forward, reload.
  std::array<gfx::Rect, 4> buttons;
  gfx::Rect omnibox;
  // One per entry of `action_widths`, in the same order; empty for a hidden
  // action.
  std::vector<gfx::Rect> actions;
};

// `action_widths` are the preferred widths of the actions from left to right,
// std::nullopt for one that is hidden. Actions are at least one hit area wide.
ToolbarGeometry LayoutToolbar(const gfx::Size& size,
                              const std::vector<std::optional<int>>& action_widths);

// Sidebar list (tokens.md section 3).
struct TabFacts {
  bool pinned = false;
  bool active = false;
  base::Time last_active;
  // Index of the group in the window's order of first appearance, if any.
  std::optional<int> group;
  bool group_collapsed = false;
  int group_tab_count = 0;
};

enum class EntryKind {
  // A section header: `section`, or the group's name when `group` is set.
  kHeader,
  // A tab row: `tab` is its model index.
  kTab,
  // The one row of a collapsed group: `tab` is its first tab.
  kGroupSummary,
};

struct SidebarEntry {
  EntryKind kind = EntryKind::kTab;
  int tab = -1;
  std::optional<sidebar::Section> section;
  std::optional<int> group;
  int group_tab_count = 0;

  bool operator==(const SidebarEntry&) const = default;
};

// The rows of the sidebar for the tabs in model order: a header whenever the
// run changes (the section of a tab on its own, or a group), the tabs, and one
// summary row instead of the tabs of a collapsed group.
std::vector<SidebarEntry> BuildSidebar(const std::vector<TabFacts>& tabs,
                                       base::Time now);

}  // namespace atlas::kit

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_LAYOUT_H_
