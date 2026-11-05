// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_LAYOUT_PROVIDER_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_LAYOUT_PROVIDER_H_

#include <optional>
#include <string_view>

#include "ui/gfx/geometry/insets.h"

namespace atlas::kit {

// The layout metrics that dialogs, sheets and bubbles ask the engine's
// ChromeLayoutProvider for, with the Design System's numbers
// (kit_metrics.h). ChromeLayoutProvider asks these first when the Kit chrome
// is on (patch 0104) and falls back to the engine's value when they answer
// nothing. `metric` is a views::InsetsMetric or views::DistanceMetric.
//
// Radii are not here: the native radii (8 control, 12 card) are already the
// Design System's through patch 0070 (atlas_tokens.h).
std::optional<gfx::Insets> KitInsetsMetric(int metric);
std::optional<int> KitDistanceMetric(int metric);

// Whether this browser process uses them (the --atlas-kit-chrome switch).
bool KitLayoutEnabled();

// The density of the Settings (density.json: compact, standard, comfortable)
// as the shell passes it with --atlas-density=NAME; standard when absent or
// unknown. Type, icons, colour and radius never change with density.
inline constexpr char kDensitySwitch[] = "atlas-density";
enum class Density { kCompact, kStandard, kComfortable };
Density ParseDensity(std::string_view name);
Density CurrentDensity();

// The height of a menu row (menu.toml, density.json "menu-item"): 24, 28, 32.
int MenuItemHeight(Density density);

// The width of the downloads card: the side sheet's 336 (side-sheet.toml).
int DownloadsCardWidth();

}  // namespace atlas::kit

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_LAYOUT_PROVIDER_H_
