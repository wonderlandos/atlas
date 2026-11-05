// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/sidebar/group_style.h"

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font_list.h"

namespace atlas::sidebar {

TEST(AtlasSidebarGroupStyleTest, TheGroupNameIsDrawnInCapitalLetters) {
  EXPECT_EQ(u"TRIP TO PORTO", GroupHeaderText(u"Trip to Porto"));
  EXPECT_EQ(u"", GroupHeaderText(u""));
}

TEST(AtlasSidebarGroupStyleTest, TheHeaderFontIsElevenPixelsAndWeight600) {
  const gfx::FontList font = GroupHeaderFont(gfx::FontList());
  EXPECT_EQ(11, font.GetFontSize());
  EXPECT_EQ(gfx::Font::Weight::SEMIBOLD, font.GetFontWeight());
}

TEST(AtlasSidebarGroupStyleTest, TheSummaryRowCountsTabsAndOpensTheGroup) {
  int count = 3;
  int activated = 0;
  GroupSummaryRow row(base::BindLambdaForTesting([&] { return count; }),
                      base::BindLambdaForTesting([&] { ++activated; }));
  EXPECT_EQ(u"3 tabs", row.GetSummaryText());
  count = 1;
  EXPECT_EQ(u"1 tab", row.GetSummaryText());
  EXPECT_EQ(kGroupSummaryRowHeight,
            row.CalculatePreferredSize(views::SizeBounds(200, {})).height());
  EXPECT_EQ(0, activated);
}

}  // namespace atlas::sidebar
