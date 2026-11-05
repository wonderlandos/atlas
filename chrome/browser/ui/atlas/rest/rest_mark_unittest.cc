// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/rest/rest_mark.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace atlas::rest {

TEST(AtlasRestMarkTest, OnlyADiscardedTabBehindRests) {
  EXPECT_TRUE(IsResting(/*discarded=*/true, /*active=*/false));
  EXPECT_FALSE(IsResting(/*discarded=*/true, /*active=*/true));
  EXPECT_FALSE(IsResting(/*discarded=*/false, /*active=*/false));
  EXPECT_FALSE(IsResting(/*discarded=*/false, /*active=*/true));
}

TEST(AtlasRestMarkTest, RestingTitleIsBetweenTextAndFill) {
  const SkColor text = SkColorSetRGB(0x15, 0x15, 0x20);
  const SkColor fill = SkColorSetRGB(0xf1, 0xf1, 0xfa);
  const SkColor dim = RestingTitleColor(text, fill);
  EXPECT_NE(dim, text);
  EXPECT_NE(dim, fill);
  EXPECT_GT(SkColorGetR(dim), SkColorGetR(text));
  EXPECT_LT(SkColorGetR(dim), SkColorGetR(fill));
}

TEST(AtlasRestMarkTest, MarkSitsAtTheTrailingEndOfARow) {
  const gfx::Rect r = RestMarkBounds(/*row_width=*/204, /*row_height=*/28);
  EXPECT_EQ(r.size(), gfx::Size(kRestMarkSize, kRestMarkSize));
  EXPECT_EQ(r.right(), 204 - kRestMarkTrailingInset);
  EXPECT_EQ(r.y(), (28 - kRestMarkSize) / 2);
}

}  // namespace atlas::rest
