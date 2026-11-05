// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_CHROME_ATLAS_TOKENS_H_
#define CHROME_BROWSER_UI_ATLAS_CHROME_ATLAS_TOKENS_H_

#include "third_party/skia/include/core/SkColor.h"

// The fixed (not themable) numbers of the WolOS chrome: tokens.md sections 2,
// 4 and 8, and the radius block of the OS design repo (wave3/int,
// tokens/colors.json: 8/12/16/12; tokens.md section 1 still says 6/10/14).
// Colours are not here: they come from the system theme (atlas_color_mixer.h).
// layout_constants.cc and layout_provider.cc hold the same numbers as literals
// next to a comment (the engine's layout code does not depend on this
// directory); atlas_tokens_unittest.cc fails when the two drift, and
// atlas/transform/tokens.py --check fails when the design repo moves.
namespace atlas::tokens {

// Window and toolbar (section 2).
inline constexpr int kToolbarHeight = 36;
inline constexpr int kToolbarButtonHitArea = 28;
inline constexpr int kToolbarGlyph = 16;
inline constexpr int kRowHeight = 28;
inline constexpr int kSidebarRowInset = 8;

// Radii (section 1).
inline constexpr int kRadiusControl = 8;
inline constexpr int kRadiusCard = 12;
inline constexpr int kRadiusLarge = 16;
inline constexpr int kRadiusWindow = 12;

// Omnibox (section 4).
// 24 px including the border (lead decision 2026-11-05, E2E-10: RU-702 allows 24/28/36/40,
// the reel measured 26).
inline constexpr int kOmniboxHeight = 24;
inline constexpr int kOmniboxMaxWidth = 560;
inline constexpr int kFocusRingThickness = 2;
inline constexpr int kProgressBarHeight = 2;

// Find bar and secondary text (section 8, section 1 type scale).
inline constexpr int kFindBarHeight = 36;
inline constexpr int kSecondaryTextSize = 11;

// Downloads (section 8).
inline constexpr int kDownloadGlyph = 16;

// Alphas of the colours derived from the system theme (percent of 255).
// `dim` is the `text` token at about 63% over the toolbar (tokens.md section 7
// pairs).
inline constexpr SkAlpha kDimAlpha = 0xA0;
// Selection pill: `accent` at 22% (light) and 30% (dark).
inline constexpr SkAlpha kSelectionAlphaLight = 0x38;
inline constexpr SkAlpha kSelectionAlphaDark = 0x4D;
// Hover on the omnibox fill: one step toward `card`.
inline constexpr SkAlpha kHoverTowardCardAlpha = 0x60;

}  // namespace atlas::tokens

#endif  // CHROME_BROWSER_UI_ATLAS_CHROME_ATLAS_TOKENS_H_
