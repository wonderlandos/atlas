// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/helium/frame_corner_radius.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/layout_provider.h"

namespace {

// layout_constants.cc and layout_provider.cc hold the token numbers as literals
// next to a comment; these tests fail when they drift from atlas_tokens.h
// (patch 0070).
class AtlasTokensTest : public testing::Test {
 protected:
  views::LayoutProvider provider_;
};

TEST_F(AtlasTokensTest, ToolbarIs36WithA28HitAreaAndA24Omnibox) {
  ui::TouchUiController::TouchUiScoperForTesting not_touch(false);
  EXPECT_EQ(GetLayoutConstant(LayoutConstant::kToolbarButtonHeight),
            atlas::tokens::kToolbarButtonHitArea);
  EXPECT_EQ(GetLayoutConstant(LayoutConstant::kLocationBarHeight),
            atlas::tokens::kOmniboxHeight);
  const gfx::Insets margin = GetLayoutInsets(TOOLBAR_INTERIOR_MARGIN);
  EXPECT_EQ(
      GetLayoutConstant(LayoutConstant::kToolbarButtonHeight) + margin.height(),
      atlas::tokens::kToolbarHeight);
}

TEST_F(AtlasTokensTest, SidebarRowsAre28WithAnInsetOf8AndTheControlRadius) {
  EXPECT_EQ(GetLayoutConstant(LayoutConstant::kVerticalTabHeight),
            atlas::tokens::kRowHeight);
  EXPECT_EQ(GetLayoutConstant(LayoutConstant::kVerticalTabPinnedHeight),
            atlas::tokens::kRowHeight);
  EXPECT_EQ(GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth),
            atlas::tokens::kRowHeight);
  EXPECT_EQ(
      GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding),
      atlas::tokens::kSidebarRowInset);
  EXPECT_EQ(GetLayoutConstant(LayoutConstant::kVerticalTabCornerRadius),
            atlas::tokens::kRadiusControl);
}

TEST_F(AtlasTokensTest, RadiiAreControls8Cards12Large16) {
  const gfx::Size size(200, 100);
  const auto radius = [&](views::ShapeContextTokens token) {
    return provider_.GetCornerRadiusMetric(token, size);
  };
  EXPECT_EQ(radius(views::ShapeContextTokens::kButtonRadius),
            atlas::tokens::kRadiusControl);
  EXPECT_EQ(radius(views::ShapeContextTokens::kTextfieldRadius),
            atlas::tokens::kRadiusControl);
  EXPECT_EQ(radius(views::ShapeContextTokens::kMenuRadius),
            atlas::tokens::kRadiusCard);
  EXPECT_EQ(radius(views::ShapeContextTokens::kDialogRadius),
            atlas::tokens::kRadiusCard);
  EXPECT_EQ(radius(views::ShapeContextTokens::kFindBarViewRadius),
            atlas::tokens::kRadiusCard);
  EXPECT_EQ(radius(views::ShapeContextTokens::kOmniboxExpandedRadius),
            atlas::tokens::kRadiusCard);
  EXPECT_EQ(provider_.GetCornerRadiusMetric(views::Emphasis::kHigh, size),
            atlas::tokens::kRadiusControl);
}

TEST_F(AtlasTokensTest, WindowRadiusIs12) {
  EXPECT_EQ(GetGenericFrameRadius(), atlas::tokens::kRadiusWindow);
  EXPECT_EQ(GetSystemWindowRadius(), atlas::tokens::kRadiusWindow);
}

TEST_F(AtlasTokensTest, DownloadGlyphIs16) {
  EXPECT_EQ(GetLayoutConstant(LayoutConstant::kDownloadIconSize),
            atlas::tokens::kDownloadGlyph);
}

}  // namespace
