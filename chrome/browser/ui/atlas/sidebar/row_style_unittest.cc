// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/sidebar/row_style.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"

namespace atlas::sidebar {

namespace {

constexpr SkColor kAccent = SkColorSetRGB(0x7c, 0x5c, 0xff);
constexpr SkColor kText = SkColorSetRGB(0x18, 0x15, 0x21);
constexpr SkColor kDim = SkColorSetRGB(0x68, 0x65, 0x75);
constexpr SkColor kControl = SkColorSetRGB(0xe9, 0xe9, 0xf3);

enum class Mode { kLight, kDark };

void Fill(ui::ColorProvider& provider, Mode mode) {
  provider.SetColorForTesting(ui::kColorAccent, kAccent);
  provider.SetColorForTesting(ui::kColorPrimaryForeground, kText);
  provider.SetColorForTesting(ui::kColorSecondaryForeground, kDim);
  provider.SetColorForTesting(ui::kColorSubtleEmphasisBackground, kControl);
  provider.SetColorForTesting(ui::kColorSeparator, SK_ColorGRAY);
  provider.SetColorForTesting(ui::kColorPrimaryBackground, mode == Mode::kDark
                                                               ? SK_ColorBLACK
                                                               : SK_ColorWHITE);
}

}  // namespace

TEST(AtlasSidebarRowStyleTest, PillIsTheAccentAtTwentyTwoPercentInLight) {
  ui::ColorProvider provider;
  Fill(provider, Mode::kLight);
  EXPECT_FALSE(IsDarkTheme(provider));
  const SkColor pill = RowFillColor(provider, RowState::kActive, 0.0);
  EXPECT_EQ(SkColorSetRGB(0x7c, 0x5c, 0xff), SkColorSetA(pill, 0xff));
  EXPECT_EQ(56u, SkColorGetA(pill));  // 0.22 * 255 = 56.1
}

TEST(AtlasSidebarRowStyleTest, PillIsTheAccentAtThirtyPercentInDark) {
  ui::ColorProvider provider;
  Fill(provider, Mode::kDark);
  EXPECT_TRUE(IsDarkTheme(provider));
  EXPECT_EQ(77u, SkColorGetA(RowFillColor(provider, RowState::kActive, 0.0)));
}

TEST(AtlasSidebarRowStyleTest, IdleRowFadesToControlWithHover) {
  ui::ColorProvider provider;
  Fill(provider, Mode::kLight);
  EXPECT_EQ(0u, SkColorGetA(RowFillColor(provider, RowState::kIdle, 0.0)));
  const SkColor half = RowFillColor(provider, RowState::kIdle, 0.5);
  EXPECT_EQ(SkColorSetA(kControl, 0xff), SkColorSetA(half, 0xff));
  EXPECT_GT(SkColorGetA(half), 0u);
  EXPECT_EQ(kControl, RowFillColor(provider, RowState::kIdle, 1.0));
}

TEST(AtlasSidebarRowStyleTest, SelectedRowKeepsItsPillWhenHovered) {
  ui::ColorProvider provider;
  Fill(provider, Mode::kLight);
  EXPECT_EQ(RowFillColor(provider, RowState::kActive, 0.0),
            RowFillColor(provider, RowState::kActive, 1.0));
  EXPECT_LT(SkColorGetA(RowFillColor(provider, RowState::kSelected, 0.0)),
            SkColorGetA(RowFillColor(provider, RowState::kActive, 0.0)));
}

TEST(AtlasSidebarRowStyleTest, TitleIsTextAndIconIsDimUntilSelected) {
  ui::ColorProvider provider;
  Fill(provider, Mode::kLight);
  EXPECT_EQ(kText, RowTitleColor(provider));
  EXPECT_EQ(kDim, RowIconColor(provider, RowState::kIdle));
  EXPECT_EQ(kAccent, RowIconColor(provider, RowState::kActive));
  EXPECT_EQ(kDim, DimTextColor(provider));
}

}  // namespace atlas::sidebar
