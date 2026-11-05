// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/sidebar/row_style.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace atlas::sidebar {

namespace {

SkColor WithShare(SkColor color, float share) {
  const float alpha = std::clamp(share, 0.0f, 1.0f) * SkColorGetA(color);
  return SkColorSetA(color, base::saturated_cast<U8CPU>(alpha + 0.5f));
}

}  // namespace

bool IsDarkTheme(const ui::ColorProvider& provider) {
  return color_utils::IsDark(provider.GetColor(ui::kColorPrimaryBackground));
}

SkColor PillColor(const ui::ColorProvider& provider, bool dark) {
  return WithShare(provider.GetColor(ui::kColorAccent),
                   dark ? kPillAlphaDark : kPillAlphaLight);
}

SkColor RowFillColor(const ui::ColorProvider& provider,
                     RowState state,
                     double hover) {
  const bool dark = IsDarkTheme(provider);
  switch (state) {
    case RowState::kActive:
      return PillColor(provider, dark);
    case RowState::kSelected:
      return WithShare(PillColor(provider, dark), kSelectedRowShare);
    case RowState::kIdle:
      return WithShare(provider.GetColor(ui::kColorSubtleEmphasisBackground),
                       static_cast<float>(std::clamp(hover, 0.0, 1.0)));
  }
  return SK_ColorTRANSPARENT;
}

SkColor RowTitleColor(const ui::ColorProvider& provider) {
  return provider.GetColor(ui::kColorPrimaryForeground);
}

SkColor RowIconColor(const ui::ColorProvider& provider, RowState state) {
  return provider.GetColor(state == RowState::kActive
                               ? ui::kColorAccent
                               : ui::kColorSecondaryForeground);
}

gfx::FontList RowTitleFont(const gfx::FontList& current, bool active) {
  return current.DeriveWithWeight(active ? gfx::Font::Weight::MEDIUM
                                         : gfx::Font::Weight::NORMAL);
}

SkColor DimTextColor(const ui::ColorProvider& provider) {
  return provider.GetColor(ui::kColorSecondaryForeground);
}

SkColor LineColor(const ui::ColorProvider& provider) {
  return provider.GetColor(ui::kColorSeparator);
}

void PaintSidebarEdge(gfx::Canvas* canvas,
                      const gfx::Rect& bounds,
                      bool on_right,
                      const ui::ColorProvider& provider) {
  canvas->FillRect(gfx::Rect(on_right ? bounds.right() - 1 : bounds.x(),
                             bounds.y(), 1, bounds.height()),
                   LineColor(provider));
}

}  // namespace atlas::sidebar
