// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/chrome/atlas_chrome_views.h"

#include <algorithm>

#include "base/i18n/case_conversion.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/controls/label.h"

namespace atlas {

bool PaintProgressBar(gfx::Canvas* canvas,
                      const gfx::SizeF& size,
                      float progress_width,
                      float fade,
                      SkColor color) {
  const float bar_height = static_cast<float>(tokens::kProgressBarHeight);
  if (size.width() <= 0.0f || size.height() < bar_height ||
      progress_width <= 0.0f) {
    return false;
  }
  const float width = std::min(progress_width, size.width());
  const float opacity = std::clamp(fade, 0.0f, 1.0f);
  cc::PaintFlags flags;
  flags.setAntiAlias(false);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(SkColorSetA(
      color, static_cast<SkAlpha>(opacity * SkColorGetA(color) + 0.5f)));
  canvas->DrawRect(
      gfx::RectF(0.0f, size.height() - bar_height, width, bar_height), flags);
  return true;
}

void StyleSectionHeader(views::Label* label) {
  label->SetText(base::i18n::ToUpper(label->GetText()));
  label->SetFontList(SecondaryFontList(label->font_list())
                         .DeriveWithWeight(gfx::Font::Weight::SEMIBOLD));
  label->SetEnabledColor(kColorOmniboxTextDimmed);
}

gfx::FontList SecondaryFontList(const gfx::FontList& base) {
  return base.DeriveWithSizeDelta(tokens::kSecondaryTextSize -
                                  base.GetFontSize());
}

}  // namespace atlas
