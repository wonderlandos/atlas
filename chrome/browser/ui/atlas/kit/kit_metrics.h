// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_METRICS_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_METRICS_H_

// The numbers of the dialogs, sheets, tooltips and menus of the WolOS Design
// System, one constant for each number of a spec in the design repo's
// components/*.toml. atlas/transform/kit_metrics.py --check DESIGN_REPO fails
// when a spec moves away from these (atlas/tests/test_kit_metrics.sh), and
// kit_layout_provider_unittest.cc holds the layout provider to them. Keep the
// names: the checker reads this file by name.
namespace atlas::kit::metrics {

// sheet.toml (our dialog): the card fill, radius 12, 16 px inside; buttons 8 px
// apart, the default one last.
inline constexpr int kSheetRadius = 12;
inline constexpr int kSheetInset = 16;
inline constexpr int kSheetButtonGap = 8;

// side-sheet.toml: a pane 336 wide (the Control Center width), the 12 px inset.
inline constexpr int kSideSheetWidth = 336;
inline constexpr int kSideSheetInset = 12;

// bottom-sheet.toml: radius 24 at the top corners, a 40 x 4 handle, the 12 px
// inset at the sides.
inline constexpr int kBottomSheetRadius = 24;
inline constexpr int kBottomSheetHandleWidth = 40;
inline constexpr int kBottomSheetHandleHeight = 4;
inline constexpr int kBottomSheetInset = 12;

// rich-tooltip.toml: radius 12, the 12 px inset, at most 40 characters a line.
inline constexpr int kRichTooltipRadius = 12;
inline constexpr int kRichTooltipInset = 12;
inline constexpr int kRichTooltipLineChars = 40;

// menu.toml: radius 12; items 24 (compact), 28 (standard), 32 (comfortable).
inline constexpr int kMenuRadius = 12;
inline constexpr int kMenuItemCompact = 24;
inline constexpr int kMenuItemStandard = 28;
inline constexpr int kMenuItemComfortable = 32;

// button.toml: radius 8, padding 0 12.
inline constexpr int kButtonRadius = 8;
inline constexpr int kButtonPadding = 12;

}  // namespace atlas::kit::metrics

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_METRICS_H_
