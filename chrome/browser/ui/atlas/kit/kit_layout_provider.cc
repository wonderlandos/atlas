// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_layout_provider.h"

#include "base/command_line.h"
#include "chrome/browser/ui/atlas/kit/kit_chrome.h"
#include "chrome/browser/ui/atlas/kit/kit_metrics.h"
#include "ui/views/layout/layout_provider.h"

namespace atlas::kit {

std::optional<gfx::Insets> KitInsetsMetric(int metric) {
  switch (metric) {
    case views::INSETS_DIALOG:
    case views::INSETS_DIALOG_SUBSECTION:
      return gfx::Insets(metrics::kSheetInset);
    default:
      return std::nullopt;
  }
}

std::optional<int> KitDistanceMetric(int metric) {
  switch (metric) {
    case views::DISTANCE_RELATED_BUTTON_HORIZONTAL:
      return metrics::kSheetButtonGap;
    case views::DISTANCE_BUTTON_HORIZONTAL_PADDING:
      return metrics::kButtonPadding;
    default:
      return std::nullopt;
  }
}

Density ParseDensity(std::string_view name) {
  if (name == "compact") {
    return Density::kCompact;
  }
  if (name == "comfortable") {
    return Density::kComfortable;
  }
  return Density::kStandard;
}

Density CurrentDensity() {
  static const Density density = [] {
    const base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    return command_line->HasSwitch(kDensitySwitch)
               ? ParseDensity(command_line->GetSwitchValueASCII(kDensitySwitch))
               : Density::kStandard;
  }();
  return density;
}

int MenuItemHeight(Density density) {
  switch (density) {
    case Density::kCompact:
      return metrics::kMenuItemCompact;
    case Density::kComfortable:
      return metrics::kMenuItemComfortable;
    case Density::kStandard:
      break;
  }
  return metrics::kMenuItemStandard;
}

int DownloadsCardWidth() {
  return metrics::kSideSheetWidth;
}

bool KitLayoutEnabled() {
  // Asked for every metric: read the switch once.
  static const bool enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kKitChromeSwitch);
  return enabled;
}

}  // namespace atlas::kit
