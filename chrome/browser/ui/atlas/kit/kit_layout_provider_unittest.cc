// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_layout_provider.h"

#include "base/command_line.h"
#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"
#include "chrome/browser/ui/atlas/kit/kit_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/layout/layout_provider.h"

namespace atlas::kit {

// The numbers themselves are held to the design repo's specs by
// atlas/transform/kit_metrics.py; these tests hold the layout provider to the
// numbers and the numbers to each other.

TEST(AtlasKitMetricsTest, DialogsAreTheSheetInsetOnAllSides) {
  for (int metric : {views::INSETS_DIALOG, views::INSETS_DIALOG_SUBSECTION}) {
    const std::optional<gfx::Insets> insets = KitInsetsMetric(metric);
    ASSERT_TRUE(insets.has_value());
    EXPECT_TRUE(*insets == gfx::Insets(metrics::kSheetInset));
  }
}

TEST(AtlasKitMetricsTest, ButtonsAreEightApartAndPaddedTwelve) {
  EXPECT_EQ(KitDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL),
            std::optional<int>(8));
  EXPECT_EQ(KitDistanceMetric(views::DISTANCE_BUTTON_HORIZONTAL_PADDING),
            std::optional<int>(12));
}

TEST(AtlasKitMetricsTest, OtherMetricsAreTheEnginesOwn) {
  EXPECT_FALSE(KitInsetsMetric(views::INSETS_DIALOG_FOOTNOTE).has_value());
  EXPECT_FALSE(KitDistanceMetric(views::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH)
                   .has_value());
}

TEST(AtlasKitMetricsTest, RadiiAgreeWithTheNativeTokens) {
  // Sheets, menus and tooltips share the card radius, buttons the control one.
  EXPECT_EQ(metrics::kSheetRadius, tokens::kRadiusCard);
  EXPECT_EQ(metrics::kMenuRadius, tokens::kRadiusCard);
  EXPECT_EQ(metrics::kRichTooltipRadius, tokens::kRadiusCard);
  EXPECT_EQ(metrics::kButtonRadius, tokens::kRadiusControl);
}

TEST(AtlasKitMetricsTest, MenuItemsFollowTheDensitiesAndTheRowToken) {
  EXPECT_LT(metrics::kMenuItemCompact, metrics::kMenuItemStandard);
  EXPECT_LT(metrics::kMenuItemStandard, metrics::kMenuItemComfortable);
  // The standard menu row is the 28 px row of the sidebar and the Kit popover.
  EXPECT_EQ(metrics::kMenuItemStandard, tokens::kRowHeight);
}

TEST(AtlasKitMetricsTest, SheetsHaveTheirSpecifiedSizes) {
  EXPECT_EQ(metrics::kSideSheetWidth, 336);
  EXPECT_EQ(metrics::kBottomSheetHandleWidth, 40);
  EXPECT_EQ(metrics::kBottomSheetHandleHeight, 4);
  EXPECT_EQ(metrics::kBottomSheetRadius, 24);
}

TEST(AtlasKitMetricsTest, DensityNamesParseAndUnknownIsStandard) {
  EXPECT_EQ(ParseDensity("compact"), Density::kCompact);
  EXPECT_EQ(ParseDensity("comfortable"), Density::kComfortable);
  EXPECT_EQ(ParseDensity("standard"), Density::kStandard);
  EXPECT_EQ(ParseDensity(""), Density::kStandard);
  EXPECT_EQ(ParseDensity("Compact"), Density::kStandard);
  EXPECT_EQ(ParseDensity("huge"), Density::kStandard);
}

TEST(AtlasKitMetricsTest, MenuRowsFollowTheDensity) {
  EXPECT_EQ(MenuItemHeight(Density::kCompact), metrics::kMenuItemCompact);
  EXPECT_EQ(MenuItemHeight(Density::kStandard), metrics::kMenuItemStandard);
  EXPECT_EQ(MenuItemHeight(Density::kComfortable),
            metrics::kMenuItemComfortable);
  EXPECT_EQ(MenuItemHeight(Density::kCompact), 24);
  EXPECT_EQ(MenuItemHeight(Density::kComfortable), 32);
  // No switch in the test process: the standard density.
  if (!base::CommandLine::InitializedForCurrentProcess()) {
    base::CommandLine::Init(0, nullptr);
  }
  EXPECT_EQ(CurrentDensity(), Density::kStandard);
}

TEST(AtlasKitMetricsTest, TheDownloadsCardIsTheSideSheetWide) {
  EXPECT_EQ(DownloadsCardWidth(), metrics::kSideSheetWidth);
  EXPECT_EQ(DownloadsCardWidth(), 336);
}

}  // namespace atlas::kit
