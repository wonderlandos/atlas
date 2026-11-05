// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_layout.h"

#include "base/time/time.h"
#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace atlas::kit {
namespace {

using sidebar::Section;

// Rects are compared as strings: the slot tree has no test fonts, so the
// binary does not link //ui/gfx:test_support (where gfx::PrintTo lives).

TEST(AtlasKitToolbarTest, ButtonsSitInTheirHitAreasFromTheLeftEdge) {
  const ToolbarGeometry g = LayoutToolbar({1280, 36}, {});
  for (size_t i = 0; i < g.buttons.size(); ++i) {
    EXPECT_EQ(g.buttons[i].ToString(),
              gfx::Rect(4 + 28 * static_cast<int>(i), 4, 28, 28).ToString());
  }
}

TEST(AtlasKitToolbarTest, OmniboxIsCentredAndAtMost560Wide) {
  const ToolbarGeometry g =
      LayoutToolbar({1280, 36}, {28, 28, 28, 28});
  EXPECT_EQ(g.omnibox.width(), tokens::kOmniboxMaxWidth);
  EXPECT_EQ(g.omnibox.height(), tokens::kOmniboxHeight);
  EXPECT_EQ(g.omnibox.CenterPoint().x(), 640);
  EXPECT_EQ(g.omnibox.height(), 24);  // incl. the border (E2E-10)
  EXPECT_EQ(g.omnibox.y(), 6);
}

TEST(AtlasKitToolbarTest, ActionsAreRightAlignedAndHiddenOnesTakeNoRoom) {
  const ToolbarGeometry g =
      LayoutToolbar({1280, 36}, {28, std::nullopt, 40, 28});
  EXPECT_EQ(g.actions[3].ToString(), gfx::Rect(1248, 4, 28, 28).ToString());
  EXPECT_EQ(g.actions[2].ToString(), gfx::Rect(1208, 4, 40, 28).ToString());
  EXPECT_TRUE(g.actions[1].IsEmpty());
  EXPECT_EQ(g.actions[0].ToString(), gfx::Rect(1180, 4, 28, 28).ToString());
}

TEST(AtlasKitToolbarTest, OmniboxShrinksToTheRoomAndNeverOverlaps) {
  const ToolbarGeometry g = LayoutToolbar({500, 36}, {28, 28, 28, 28});
  EXPECT_GE(g.omnibox.x(), g.buttons[3].right() + kOmniboxGap);
  EXPECT_LE(g.omnibox.right(), g.actions[0].x() - kOmniboxGap);
  EXPECT_LT(g.omnibox.width(), tokens::kOmniboxMaxWidth);
}

TEST(AtlasKitSidebarTest, PinnedTodayYesterdayAndGroups) {
  const base::Time now = base::Time::Now();
  const base::Time yesterday = (now.LocalMidnight() - base::Hours(12));
  std::vector<TabFacts> tabs(6);
  tabs[0].pinned = true;
  tabs[1].active = true;
  tabs[2].last_active = now;
  tabs[3].last_active = yesterday;
  tabs[4].group = 0;
  tabs[4].group_tab_count = 2;
  tabs[5].group = 0;
  tabs[5].group_tab_count = 2;

  const std::vector<SidebarEntry> e = BuildSidebar(tabs, now);
  const std::vector<SidebarEntry> want = {
      {.kind = EntryKind::kHeader, .section = Section::kPinned},
      {.kind = EntryKind::kTab, .tab = 0},
      {.kind = EntryKind::kHeader, .section = Section::kToday},
      {.kind = EntryKind::kTab, .tab = 1},
      {.kind = EntryKind::kTab, .tab = 2},
      {.kind = EntryKind::kHeader, .section = Section::kYesterday},
      {.kind = EntryKind::kTab, .tab = 3},
      {.kind = EntryKind::kHeader, .group = 0},
      {.kind = EntryKind::kTab, .tab = 4},
      {.kind = EntryKind::kTab, .tab = 5},
  };
  EXPECT_EQ(e, want);
}

TEST(AtlasKitSidebarTest, CollapsedGroupIsOneSummaryRow) {
  const base::Time now = base::Time::Now();
  std::vector<TabFacts> tabs(4);
  tabs[0].active = true;
  for (int i = 1; i < 4; ++i) {
    tabs[i].group = 0;
    tabs[i].group_collapsed = true;
    tabs[i].group_tab_count = 3;
  }
  const std::vector<SidebarEntry> e = BuildSidebar(tabs, now);
  ASSERT_EQ(e.size(), 4u);
  EXPECT_EQ(e[2], (SidebarEntry{.kind = EntryKind::kHeader, .group = 0}));
  EXPECT_EQ(e[3], (SidebarEntry{.kind = EntryKind::kGroupSummary,
                                .tab = 1,
                                .group = 0,
                                .group_tab_count = 3}));
}

TEST(AtlasKitSidebarTest, ATabAfterAGroupGetsItsHeaderAgain) {
  const base::Time now = base::Time::Now();
  std::vector<TabFacts> tabs(3);
  tabs[0].active = true;
  tabs[1].group = 0;
  tabs[1].group_tab_count = 1;
  tabs[2].last_active = now;
  const std::vector<SidebarEntry> e = BuildSidebar(tabs, now);
  ASSERT_EQ(e.size(), 6u);
  EXPECT_EQ(e[4], (SidebarEntry{.kind = EntryKind::kHeader,
                                .section = Section::kToday}));
}

TEST(AtlasKitSidebarTest, EarlierTabs) {
  const base::Time now = base::Time::Now();
  std::vector<TabFacts> tabs(2);
  tabs[0].active = true;
  tabs[1].last_active = now - base::Days(5);
  const std::vector<SidebarEntry> e = BuildSidebar(tabs, now);
  ASSERT_EQ(e.size(), 4u);
  EXPECT_EQ(e[2], (SidebarEntry{.kind = EntryKind::kHeader,
                                .section = Section::kEarlier}));
}

}  // namespace
}  // namespace atlas::kit
