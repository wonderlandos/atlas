// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pixel tests: they paint on a canvas, which needs the graphics stack of //chrome/test:unit_tests.

#include "chrome/browser/ui/atlas/sidebar/row_style.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"

namespace atlas::sidebar {

TEST(AtlasSidebarRowPaintTest, TheEdgeIsOnePixelOnTheSideFacingThePage) {
  ui::ColorProvider provider;
  provider.SetColorForTesting(ui::kColorSeparator, SK_ColorGRAY);
  gfx::Canvas canvas(gfx::Size(220, 40), 1.0f, /*is_opaque=*/true);
  canvas.DrawColor(SK_ColorWHITE);
  PaintSidebarEdge(&canvas, gfx::Rect(0, 0, 220, 40), /*on_right=*/true,
                   provider);
  SkBitmap bitmap = canvas.GetBitmap();
  EXPECT_EQ(SK_ColorGRAY, bitmap.getColor(219, 10));
  EXPECT_EQ(SK_ColorWHITE, bitmap.getColor(218, 10));
  EXPECT_EQ(SK_ColorWHITE, bitmap.getColor(0, 10));
}

}  // namespace atlas::sidebar
