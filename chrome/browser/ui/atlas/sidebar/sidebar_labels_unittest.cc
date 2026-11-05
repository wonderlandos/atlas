// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that need the string resources: they run in //chrome/test:unit_tests, whose suite loads them.

#include "chrome/browser/ui/atlas/sidebar/sidebar_sections.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace atlas::sidebar {

TEST(AtlasSidebarLabelsTest, LabelsAreCapitalLetters) {
  EXPECT_EQ(u"TODAY", SectionLabel(Section::kToday));
  EXPECT_EQ(u"YESTERDAY", SectionLabel(Section::kYesterday));
  EXPECT_EQ(u"EARLIER", SectionLabel(Section::kEarlier));
  EXPECT_EQ(u"PINNED", SectionLabel(Section::kPinned));
}

}  // namespace atlas::sidebar
