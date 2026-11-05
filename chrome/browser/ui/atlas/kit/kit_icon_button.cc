// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_icon_button.h"

#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace atlas::kit {

KitIconButton::KitIconButton(PressedCallback callback,
                             const gfx::VectorIcon& icon,
                             const std::u16string& tooltip)
    : views::Button(std::move(callback)), icon_(icon) {
  SetTooltipText(tooltip);
  GetViewAccessibility().SetName(tooltip);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                tokens::kRadiusControl);
  views::FocusRing::Install(this);
}

KitIconButton::~KitIconButton() = default;

void KitIconButton::SetIcon(const gfx::VectorIcon& icon) {
  icon_ = raw_ref(icon);
  SchedulePaint();
}

void KitIconButton::SetTooltip(const std::u16string& tooltip) {
  SetTooltipText(tooltip);
  GetViewAccessibility().SetName(tooltip);
}

gfx::Size KitIconButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(tokens::kToolbarButtonHitArea,
                   tokens::kToolbarButtonHitArea);
}

void KitIconButton::OnPaintBackground(gfx::Canvas* canvas) {
  const ui::ColorProvider* provider = GetColorProvider();
  if (!provider) {
    return;
  }
  std::optional<SkColor> fill;
  if (GetState() == STATE_PRESSED) {
    fill = provider->GetColor(kColorToolbarInkDropRipple);
  } else if (GetState() == STATE_HOVERED) {
    fill = provider->GetColor(kColorToolbarInkDropHover);
  }
  if (!fill) {
    return;
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(*fill);
  canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), tokens::kRadiusControl,
                        flags);
}

void KitIconButton::PaintButtonContents(gfx::Canvas* canvas) {
  const ui::ColorProvider* provider = GetColorProvider();
  if (!provider) {
    return;
  }
  const SkColor color =
      provider->GetColor(GetState() == STATE_DISABLED
                             ? kColorToolbarButtonIconInactive
                             : kColorToolbarButtonIcon);
  const gfx::ImageSkia image =
      gfx::CreateVectorIcon(*icon_, tokens::kToolbarGlyph, color);
  canvas->DrawImageInt(image, (width() - image.width()) / 2,
                       (height() - image.height()) / 2);
}

void KitIconButton::StateChanged(ButtonState old_state) {
  views::Button::StateChanged(old_state);
  SchedulePaint();
}

BEGIN_METADATA(KitIconButton)
END_METADATA

}  // namespace atlas::kit
