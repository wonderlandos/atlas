// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_text.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace atlas::kit {

TEST(AtlasKitTextTest, TheInheritedNamesBecomeAtlas) {
  EXPECT_EQ(DoctrineClean(u"Customize Helium"), u"Customize Atlas");
  EXPECT_EQ(DoctrineClean(u"About Chromium"), u"About Atlas");
  EXPECT_EQ(DoctrineClean(u"Customize and control Chrome"),
            u"Customize and control Atlas");
  EXPECT_EQ(DoctrineClean(u"&Helium help"), u"&Atlas help");
}

TEST(AtlasKitTextTest, OtherTextIsKept) {
  EXPECT_EQ(DoctrineClean(u"New tab"), u"New tab");
  EXPECT_EQ(DoctrineClean(u""), u"");
  // Part of a longer word: not the name.
  EXPECT_EQ(DoctrineClean(u"Chromecast"), u"Chromecast");
  EXPECT_EQ(DoctrineClean(u"Cast, save, and share"), u"Cast, save, and share");
}

TEST(AtlasKitTextTest, EveryOccurrenceIsReplaced) {
  EXPECT_EQ(DoctrineClean(u"Helium and Helium"), u"Atlas and Atlas");
}

}  // namespace atlas::kit
