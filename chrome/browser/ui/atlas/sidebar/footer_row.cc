// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/sidebar/footer_row.h"

#include "chrome/browser/ui/atlas/sidebar/row_style.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"

namespace atlas::sidebar {

namespace {

constexpr int kShortcutFontSize = 11;

}  // namespace

bool FooterShowsDecorations(int width) {
  return width >= kFooterMinWidthForText;
}

void PaintFooterDecorations(gfx::Canvas* canvas,
                            const gfx::Rect& bounds,
                            int row_inset,
                            bool rtl,
                            const ui::ColorProvider& provider) {
  if (!FooterShowsDecorations(bounds.width())) {
    return;
  }
  canvas->FillRect(
      gfx::Rect(bounds.x() + row_inset, bounds.y(),
                bounds.width() - 2 * row_inset, kFooterRuleThickness),
      LineColor(provider));

  const gfx::FontList base_font;
  const gfx::FontList font = base_font.DeriveWithSizeDelta(
      kShortcutFontSize - base_font.GetFontSize());
  const gfx::Rect text(bounds.x() + kFooterShortcutInset, bounds.y(),
                       bounds.width() - 2 * kFooterShortcutInset,
                       bounds.height());
  canvas->DrawStringRectWithFlags(
      l10n_util::GetStringUTF16(IDS_ATLAS_SIDEBAR_NEW_TAB_SHORTCUT), font,
      DimTextColor(provider), text,
      (rtl ? gfx::Canvas::TEXT_ALIGN_LEFT : gfx::Canvas::TEXT_ALIGN_RIGHT) |
          gfx::Canvas::NO_SUBPIXEL_RENDERING);
}

}  // namespace atlas::sidebar
