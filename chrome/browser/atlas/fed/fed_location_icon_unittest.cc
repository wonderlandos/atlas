// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/atlas/fed/fed_location_icon.h"

#include "chrome/browser/atlas/fed/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace atlas_fed {

TEST(AtlasFedLocationIconTest, VerifiedPageShowsTheFederateMark) {
  EXPECT_EQ(LocationIconFor(GURL("fed://home.fed/"), 200), &kFedMarkIcon);
}

TEST(AtlasFedLocationIconTest, RefusedPageShowsTheDangerousIcon) {
  EXPECT_EQ(LocationIconFor(GURL("fed://home.fed/"), 502),
            &vector_icons::kDangerousFilledIcon);
}

TEST(AtlasFedLocationIconTest, OtherAnswersAndOtherSchemesKeepTheEnginesIcon) {
  EXPECT_EQ(LocationIconFor(GURL("fed://home.fed/"), 404), nullptr);
  EXPECT_EQ(LocationIconFor(GURL("fed://home.fed/"), 0), nullptr);
  EXPECT_EQ(LocationIconFor(GURL("https://home.fed/"), 200), nullptr);
  EXPECT_EQ(LocationIconFor(GURL("https://example.com/"), 502), nullptr);
}

}  // namespace atlas_fed
