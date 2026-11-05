// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/sidebar/footer_row.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"

namespace atlas::sidebar {

TEST(AtlasSidebarFooterRowTest, ACollapsedFooterShowsTheGlyphAlone) {
  EXPECT_FALSE(FooterShowsDecorations(42));
  EXPECT_FALSE(FooterShowsDecorations(kFooterMinWidthForText - 1));
  EXPECT_TRUE(FooterShowsDecorations(220));
}

TEST(AtlasSidebarFooterRowTest, TheRuleIsOnePixelAtTheTopInsideTheRowInset) {
  ui::ColorProvider provider;
  provider.SetColorForTesting(ui::kColorSeparator, SK_ColorRED);
  provider.SetColorForTesting(ui::kColorSecondaryForeground, SK_ColorGRAY);

  gfx::Canvas canvas(gfx::Size(220, 28), 1.0f, /*is_opaque=*/true);
  canvas.DrawColor(SK_ColorWHITE);
  PaintFooterDecorations(&canvas, gfx::Rect(0, 0, 220, 28), 8, /*rtl=*/false,
                         provider);
  SkBitmap bitmap = canvas.GetBitmap();
  // Inside the inset: the rule. Outside it and one row down: untouched.
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(110, 0));
  EXPECT_EQ(SK_ColorWHITE, bitmap.getColor(3, 0));
  EXPECT_EQ(SK_ColorWHITE, bitmap.getColor(110, 1));
}

}  // namespace atlas::sidebar
