// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_layout.h"

#include <algorithm>

#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"

namespace atlas::kit {

ToolbarGeometry LayoutToolbar(
    const gfx::Size& size,
    const std::vector<std::optional<int>>& action_widths) {
  ToolbarGeometry g;
  const int hit = tokens::kToolbarButtonHitArea;
  const int top = (size.height() - hit) / 2;

  int left = kToolbarEdgeInset;
  for (gfx::Rect& button : g.buttons) {
    button = gfx::Rect(left, top, hit, hit);
    left += hit;
  }

  g.actions.resize(action_widths.size());
  int right = size.width() - kToolbarEdgeInset;
  for (size_t i = action_widths.size(); i-- > 0;) {
    if (!action_widths[i]) {
      continue;
    }
    const int w = std::max(hit, *action_widths[i]);
    right -= w;
    g.actions[i] = gfx::Rect(right, top, w, hit);
  }

  // Centred in the window, at most 560 wide, shrinking to the room between
  // the two groups.
  const int room_left = left + kOmniboxGap;
  const int room_right = right - kOmniboxGap;
  const int w =
      std::clamp(room_right - room_left, 0, tokens::kOmniboxMaxWidth);
  const int x = std::clamp((size.width() - w) / 2, room_left,
                           std::max(room_left, room_right - w));
  const int h = tokens::kOmniboxHeight;
  g.omnibox = gfx::Rect(x, (size.height() - h) / 2, w, h);
  return g;
}

std::vector<SidebarEntry> BuildSidebar(const std::vector<TabFacts>& tabs,
                                       base::Time now) {
  std::vector<SidebarEntry> out;
  std::optional<sidebar::Section> last_section;
  std::optional<int> last_group;
  for (size_t i = 0; i < tabs.size(); ++i) {
    const TabFacts& t = tabs[i];
    const int index = static_cast<int>(i);
    if (t.group) {
      if (t.group != last_group) {
        out.push_back({.kind = EntryKind::kHeader, .group = t.group});
        last_group = t.group;
        last_section.reset();
        if (t.group_collapsed) {
          out.push_back({.kind = EntryKind::kGroupSummary,
                         .tab = index,
                         .group = t.group,
                         .group_tab_count = t.group_tab_count});
        }
      }
      if (!t.group_collapsed) {
        out.push_back({.kind = EntryKind::kTab, .tab = index});
      }
      continue;
    }
    const sidebar::Section section =
        t.pinned ? sidebar::Section::kPinned
                 : sidebar::SectionOf(t.active, t.last_active, now);
    if (last_group || section != last_section) {
      out.push_back({.kind = EntryKind::kHeader, .section = section});
      last_section = section;
      last_group.reset();
    }
    out.push_back({.kind = EntryKind::kTab, .tab = index});
  }
  return out;
}

}  // namespace atlas::kit
