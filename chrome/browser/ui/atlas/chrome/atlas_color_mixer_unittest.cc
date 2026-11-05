// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/chrome/atlas_color_mixer.h"

#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_utils.h"

namespace {

// Stand-ins for what the system theme gives: distinct opaque colours so a wrong
// mapping shows.
constexpr SkColor kText = SkColorSetRGB(0x18, 0x15, 0x21);
constexpr SkColor kAccent = SkColorSetRGB(0x7C, 0x5C, 0xFF);
constexpr SkColor kOnAccent = SK_ColorWHITE;
constexpr SkColor kGlass = SkColorSetRGB(0xF4, 0xF3, 0xFA);
constexpr SkColor kCard = SK_ColorWHITE;
constexpr SkColor kControl = SkColorSetRGB(0xE9, 0xE9, 0xF3);
constexpr SkColor kLine = SkColorSetARGB(0x1A, 0x22, 0x1B, 0x36);

ui::ColorProviderKey GtkKey(bool dark) {
  ui::ColorProviderKey key;
  key.system_theme = ui::SystemTheme::kGtk;
  key.color_mode = dark ? ui::ColorProviderKey::ColorMode::kDark
                        : ui::ColorProviderKey::ColorMode::kLight;
  return key;
}

void AddSystemTheme(ui::ColorProvider& provider) {
  ui::ColorMixer& theme = provider.AddMixer();
  theme[ui::kColorNativeLabelForeground] = {kText};
  theme[ui::kColorAccent] = {kAccent};
  theme[ui::kColorButtonForegroundProminent] = {kOnAccent};
  theme[kColorToolbar] = {kGlass};
  theme[ui::kColorMenuBackground] = {kCard};
  theme[ui::kColorTextfieldBackground] = {kControl};
  theme[ui::kColorMidground] = {kLine};
}

class AtlasColorMixerTest : public testing::Test {
 protected:
  void Add(bool dark) {
    AddSystemTheme(provider_);
    atlas::AddAtlasColorMixer(&provider_, GtkKey(dark));
  }
  ui::ColorProvider provider_;
};

TEST_F(AtlasColorMixerTest, OmniboxIsControlWithALineAndAnAccentRing) {
  Add(/*dark=*/false);
  EXPECT_EQ(provider_.GetColor(kColorLocationBarBackground), kControl);
  EXPECT_EQ(provider_.GetColor(kColorLocationBarBorder), kLine);
  EXPECT_EQ(provider_.GetColor(ui::kColorFocusableBorderFocused), kAccent);
  // Hover moves the fill toward the card: it must be lighter than the control.
  EXPECT_GT(color_utils::GetRelativeLuminance(
                provider_.GetColor(kColorLocationBarBackgroundHovered)),
            color_utils::GetRelativeLuminance(kControl));
}

TEST_F(AtlasColorMixerTest, DimIsTextOverGlassAtTheTokenAlpha) {
  Add(/*dark=*/false);
  const SkColor dim = provider_.GetColor(kColorOmniboxTextDimmed);
  EXPECT_EQ(dim,
            color_utils::AlphaBlend(kText, kGlass, atlas::tokens::kDimAlpha));
  // Readable on the toolbar (tokens.md section 7: dim on glass is 5.2:1 in
  // light).
  EXPECT_GE(color_utils::GetContrastRatio(dim, kGlass), 4.5);
  EXPECT_EQ(provider_.GetColor(kColorOmniboxText), kText);
}

TEST_F(AtlasColorMixerTest, SelectionIsAccentAt22PercentLightAndAt30Dark) {
  for (bool dark : {false, true}) {
    ui::ColorProvider provider;
    AddSystemTheme(provider);
    atlas::AddAtlasColorMixer(&provider, GtkKey(dark));
    const SkColor row = provider.GetColor(kColorDownloadBubbleRowHover);
    EXPECT_EQ(SkColorSetA(row, SK_AlphaOPAQUE), kAccent);
    EXPECT_EQ(SkColorGetA(row), dark ? atlas::tokens::kSelectionAlphaDark
                                     : atlas::tokens::kSelectionAlphaLight);
  }
  // 22% and 30% of 255, rounded.
  EXPECT_NEAR(atlas::tokens::kSelectionAlphaLight / 255.0, 0.22, 0.005);
  EXPECT_NEAR(atlas::tokens::kSelectionAlphaDark / 255.0, 0.30, 0.005);
}

TEST_F(AtlasColorMixerTest, MenuHighlightIsSolidAccentWithTheOnAccentLabel) {
  Add(/*dark=*/false);
  EXPECT_EQ(provider_.GetColor(ui::kColorMenuItemBackgroundSelected), kAccent);
  EXPECT_EQ(provider_.GetColor(ui::kColorMenuItemForegroundSelected),
            kOnAccent);
  EXPECT_EQ(provider_.GetColor(ui::kColorMenuSeparator), kLine);
}

TEST_F(AtlasColorMixerTest, FindBarIsGlassAndTheDownloadRingIsAccent) {
  Add(/*dark=*/true);
  EXPECT_EQ(provider_.GetColor(kColorFindBarBackground), kGlass);
  EXPECT_EQ(provider_.GetColor(kColorDownloadToolbarButtonActive), kAccent);
  EXPECT_EQ(provider_.GetColor(kColorToolbarContentAreaSeparator), kLine);
}

TEST_F(AtlasColorMixerTest, OtherThemesKeepTheirColours) {
  ui::ColorProviderKey key;  // The default (non GTK) system theme.
  atlas::AddAtlasColorMixer(&provider_, key);
  provider_.GenerateColorMapForTesting();
  EXPECT_NE(provider_.GetColor(kColorLocationBarBackground), kControl);
}

}  // namespace
