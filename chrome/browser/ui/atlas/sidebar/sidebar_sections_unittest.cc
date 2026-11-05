// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/sidebar/sidebar_sections.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace atlas::sidebar {

namespace {

// A local time on a given day, so the tests hold in any time zone.
base::Time Local(int month, int day, int hour, int minute = 0) {
  base::Time::Exploded exploded = {};
  exploded.year = 2026;
  exploded.month = month;
  exploded.day_of_month = day;
  exploded.hour = hour;
  exploded.minute = minute;
  base::Time result;
  EXPECT_TRUE(base::Time::FromLocalExploded(exploded, &result));
  return result;
}

SectionRow Tab(base::Time last_active, bool active = false) {
  return {RowKind::kTab, active, last_active};
}

}  // namespace

TEST(AtlasSidebarSectionsTest, SectionBoundariesAreLocalMidnights) {
  const base::Time now = Local(11, 4, 15);
  EXPECT_EQ(Section::kToday, SectionOf(false, Local(11, 4, 0, 0), now));
  EXPECT_EQ(Section::kToday, SectionOf(false, Local(11, 4, 14, 59), now));
  EXPECT_EQ(Section::kYesterday, SectionOf(false, Local(11, 3, 23, 59), now));
  EXPECT_EQ(Section::kYesterday, SectionOf(false, Local(11, 3, 0, 0), now));
  EXPECT_EQ(Section::kEarlier, SectionOf(false, Local(11, 2, 23, 59), now));
  EXPECT_EQ(Section::kEarlier, SectionOf(false, Local(1, 1, 8), now));
}

TEST(AtlasSidebarSectionsTest, TheTabInFrontAndATabWithNoTimeAreToday) {
  const base::Time now = Local(11, 4, 15);
  EXPECT_EQ(Section::kToday, SectionOf(true, Local(10, 1, 8), now));
  EXPECT_EQ(Section::kToday, SectionOf(false, base::Time(), now));
}

TEST(AtlasSidebarSectionsTest, TheFirstTabGetsAHeaderAndARunSharesIt) {
  const base::Time now = Local(11, 4, 15);
  const auto headers = SectionHeaders(
      {Tab(Local(11, 4, 9)), Tab(Local(11, 4, 10)), Tab(Local(11, 4, 11))},
      now);
  ASSERT_EQ(3u, headers.size());
  EXPECT_EQ(Section::kToday, headers[0]);
  EXPECT_FALSE(headers[1]);
  EXPECT_FALSE(headers[2]);
}

TEST(AtlasSidebarSectionsTest, AHeaderAppearsWhereTheSectionChanges) {
  const base::Time now = Local(11, 4, 15);
  const auto headers = SectionHeaders(
      {Tab(Local(11, 4, 9)), Tab(Local(11, 4, 10)), Tab(Local(11, 3, 20)),
       Tab(Local(10, 20, 8)), Tab(Local(11, 4, 12))},
      now);
  ASSERT_EQ(5u, headers.size());
  EXPECT_EQ(Section::kToday, headers[0]);
  EXPECT_FALSE(headers[1]);
  EXPECT_EQ(Section::kYesterday, headers[2]);
  EXPECT_EQ(Section::kEarlier, headers[3]);
  // The order is the model's: a later tab of today gets its header again.
  EXPECT_EQ(Section::kToday, headers[4]);
}

TEST(AtlasSidebarSectionsTest, AGroupEndsTheRunAndGetsNoSectionHeader) {
  const base::Time now = Local(11, 4, 15);
  const auto headers = SectionHeaders({Tab(Local(11, 4, 9)),
                                       {RowKind::kGroup, false, base::Time()},
                                       Tab(Local(11, 4, 10))},
                                      now);
  ASSERT_EQ(3u, headers.size());
  EXPECT_EQ(Section::kToday, headers[0]);
  EXPECT_FALSE(headers[1]);
  EXPECT_EQ(Section::kToday, headers[2]);
}

TEST(AtlasSidebarSectionsTest, OtherRowsAreNeutral) {
  const base::Time now = Local(11, 4, 15);
  const auto headers = SectionHeaders({Tab(Local(11, 4, 9)),
                                       {RowKind::kOther, false, base::Time()},
                                       Tab(Local(11, 4, 10))},
                                      now);
  EXPECT_EQ(Section::kToday, headers[0]);
  EXPECT_FALSE(headers[1]);
  EXPECT_FALSE(headers[2]);
}

TEST(AtlasSidebarSectionsTest, TheActiveTabIsTodayEvenWhenItsTimeIsOld) {
  const base::Time now = Local(11, 4, 15);
  const auto headers =
      SectionHeaders({Tab(Local(10, 1, 9), /*active=*/true)}, now);
  EXPECT_EQ(Section::kToday, headers[0]);
}

TEST(AtlasSidebarSectionsTest, NoRowsNoHeaders) {
  EXPECT_TRUE(SectionHeaders({}, Local(11, 4, 15)).empty());
}

TEST(AtlasSidebarSectionsTest, BandHeights) {
  EXPECT_EQ(0, HeaderBandHeight(false, true));
  EXPECT_EQ(0, HeaderBandHeight(false, false));
  EXPECT_EQ(kHeaderHeight, HeaderBandHeight(true, false));
  EXPECT_EQ(kHeaderHeight + kHeaderGap, HeaderBandHeight(true, true));
}

}  // namespace atlas::sidebar
