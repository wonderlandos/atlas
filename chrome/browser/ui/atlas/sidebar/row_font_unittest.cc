// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Needs the font system: runs in //chrome/test:unit_tests.

#include "chrome/browser/ui/atlas/sidebar/row_style.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font_list.h"

namespace atlas::sidebar {

TEST(AtlasSidebarRowFontTest, TitleWeightIs500ForTheTabInFront) {
  const gfx::FontList base;
  EXPECT_EQ(gfx::Font::Weight::MEDIUM,
            RowTitleFont(base, true).GetFontWeight());
  EXPECT_EQ(gfx::Font::Weight::NORMAL,
            RowTitleFont(base, false).GetFontWeight());
}

}  // namespace atlas::sidebar
