// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_text.h"

#include "chrome/browser/ui/atlas/kit/kit_metrics.h"
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

// E2E-05: one notation for a shortcut, the WolOS shell's symbols (Command = Super). The sidebar
// footer's string (IDS_ATLAS_SIDEBAR_NEW_TAB_SHORTCUT) is this function's output for the engine's
// new-tab binding (Ctrl+T), checked by atlas/tests/sidebar.sh.
TEST(AtlasKitTextTest, ShortcutsAreWrittenWithTheShellsSymbols) {
  EXPECT_EQ(ShortcutSymbols(true, false, false, u"T"), u"\u2318T");
  EXPECT_EQ(ShortcutSymbols(true, false, true, u"N"), u"\u21E7\u2318N");
  EXPECT_EQ(ShortcutSymbols(false, true, false, u"Home"), u"\u2325Home");
  EXPECT_EQ(ShortcutSymbols(true, true, true, u"I"), u"\u2325\u21E7\u2318I");
  EXPECT_EQ(ShortcutSymbols(false, false, false, u"F5"), u"F5");
}

// E2E-12: the product says "rest", the engine's menu says "Hibernate".
TEST(AtlasKitTextTest, HibernateIsRest) {
  EXPECT_EQ(DoctrineClean(u"Hibernate"), u"Rest");
  EXPECT_EQ(DoctrineClean(u"Hibernate other tabs"), u"Rest other tabs");
  EXPECT_EQ(DoctrineClean(u"Hibernated"), u"Hibernated");
  EXPECT_EQ(DoctrineClean(u"Pin"), u"Pin");
}

TEST(AtlasKitTextTest, EveryOccurrenceIsReplaced) {
  EXPECT_EQ(DoctrineClean(u"Helium and Helium"), u"Atlas and Atlas");
}

TEST(AtlasKitTextTest, TooltipLinesAreAtMostTheRichTooltipWidth) {
  const std::u16string title =
      u"Trip to Porto: train times, hotels near the river and a long list of "
      u"places to eat on Sunday";
  const std::u16string wrapped =
      WrapToLineChars(title, metrics::kRichTooltipLineChars);
  size_t line = 0;
  size_t lines = 1;
  for (char16_t c : wrapped) {
    if (c == u'\n') {
      ++lines;
      line = 0;
      continue;
    }
    ++line;
    EXPECT_LE(line, static_cast<size_t>(metrics::kRichTooltipLineChars));
  }
  EXPECT_GT(lines, 1u);
  // Only spaces became newlines.
  std::u16string unwrapped = wrapped;
  for (char16_t& c : unwrapped) {
    if (c == u'\n') {
      c = u' ';
    }
  }
  EXPECT_EQ(unwrapped, title);
}

TEST(AtlasKitTextTest, ShortTextAndLongWordsAreKept) {
  EXPECT_EQ(WrapToLineChars(u"New tab", 40), u"New tab");
  EXPECT_EQ(WrapToLineChars(u"", 40), u"");
  const std::u16string word(60, u'x');
  EXPECT_EQ(WrapToLineChars(u"a " + word + u" b", 40),
            u"a\n" + word + u"\nb");
}

}  // namespace atlas::kit
