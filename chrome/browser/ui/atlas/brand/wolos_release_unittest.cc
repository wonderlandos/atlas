// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/brand/wolos_release.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace atlas::brand {

TEST(WolosReleaseTest, WolosReleaseFromVersionId) {
  EXPECT_EQ("0.13", ParseWolosRelease("NAME=WolOS\nID=wolos\nID_LIKE=arch\n"
                                      "VERSION_ID=0.13\nVERSION=\"0.13\"\n"));
}

TEST(WolosReleaseTest, WolosReleaseQuotedAndCaseInsensitive) {
  EXPECT_EQ("2026.11.04",
            ParseWolosRelease("ID=\"WolOS\"\nVERSION_ID=\"2026.11.04\"\n"));
}

TEST(WolosReleaseTest, WolosReleaseFallsBackToVersion) {
  EXPECT_EQ("0.13", ParseWolosRelease("ID=wolos\nVERSION=\"0.13\"\n"));
}

TEST(WolosReleaseTest, NotWolosGivesNothing) {
  EXPECT_EQ("", ParseWolosRelease("NAME=Arch Linux\nID=arch\nVERSION_ID=1\n"));
  EXPECT_EQ("", ParseWolosRelease("ID_LIKE=wolos\nVERSION_ID=1\n"));
  EXPECT_EQ("", ParseWolosRelease(""));
}

TEST(WolosReleaseTest, WolosWithoutAReleaseGivesNothing) {
  EXPECT_EQ("", ParseWolosRelease("ID=wolos\n"));
}

}  // namespace atlas::brand
