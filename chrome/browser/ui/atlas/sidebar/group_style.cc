// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/sidebar/group_style.h"

#include "base/i18n/case_conversion.h"
#include "chrome/browser/ui/atlas/sidebar/row_style.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace atlas::sidebar {

namespace {

constexpr int kHeaderFontSize = 11;
constexpr int kGlyphSize = 16;
// The glyph and the text line up with the icon and the title of a tab row.
constexpr int kGlyphLeft = 7;
constexpr int kTextLeft = 31;
constexpr int kRowRadius = 6;

// A bookmark ribbon on a 16 px grid.
SkPath BookmarkGlyph(const gfx::Point& origin) {
  const float x = static_cast<float>(origin.x());
  const float y = static_cast<float>(origin.y());
  SkPathBuilder path;
  path.moveTo(x + 3.5f, y + 1.5f);
  path.lineTo(x + 12.5f, y + 1.5f);
  path.lineTo(x + 12.5f, y + 14.5f);
  path.lineTo(x + 8.0f, y + 10.5f);
  path.lineTo(x + 3.5f, y + 14.5f);
  path.close();
  return path.detach();
}

}  // namespace

std::u16string GroupHeaderText(const std::u16string& title) {
  return base::i18n::ToUpper(title);
}

gfx::FontList GroupHeaderFont(const gfx::FontList& current) {
  return current.DeriveWithSizeDelta(kHeaderFontSize - current.GetFontSize())
      .DeriveWithWeight(gfx::Font::Weight::SEMIBOLD);
}

GroupSummaryRow::GroupSummaryRow(base::RepeatingCallback<int()> tab_count,
                                 base::RepeatingClosure on_activate)
    : tab_count_(std::move(tab_count)), on_activate_(std::move(on_activate)) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  UpdateAccessibleName();
}

GroupSummaryRow::~GroupSummaryRow() = default;

std::u16string GroupSummaryRow::GetSummaryText() const {
  return l10n_util::GetPluralStringFUTF16(IDS_ATLAS_SIDEBAR_GROUP_TAB_COUNT,
                                          tab_count_.Run());
}

gfx::Size GroupSummaryRow::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(available_size.width().value_or(0), kGroupSummaryRowHeight);
}

void GroupSummaryRow::UpdateAccessibleName() {
  const std::u16string text = GetSummaryText();
  if (text != accessible_name_) {
    accessible_name_ = text;
    GetViewAccessibility().SetName(text);
  }
}

void GroupSummaryRow::OnPaint(gfx::Canvas* canvas) {
  const ui::ColorProvider* provider = GetColorProvider();
  if (!provider) {
    return;
  }
  UpdateAccessibleName();
  if (hovered_) {
    cc::PaintFlags fill;
    fill.setAntiAlias(true);
    fill.setColor(RowFillColor(*provider, RowState::kIdle, 1.0));
    canvas->DrawRoundRect(GetLocalBounds(), kRowRadius, fill);
  }
  const SkColor dim = DimTextColor(*provider);
  cc::PaintFlags glyph;
  glyph.setAntiAlias(true);
  glyph.setColor(dim);
  glyph.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(
      BookmarkGlyph(gfx::Point(kGlyphLeft, (height() - kGlyphSize) / 2)),
      glyph);

  const gfx::FontList base_font;
  const gfx::FontList font =
      base_font.DeriveWithSizeDelta(13 - base_font.GetFontSize());
  canvas->DrawStringRectWithFlags(
      accessible_name_, font, dim,
      gfx::Rect(kTextLeft, 0, width() - kTextLeft, height()),
      gfx::Canvas::TEXT_ALIGN_LEFT | gfx::Canvas::NO_SUBPIXEL_RENDERING);
}

bool GroupSummaryRow::OnMousePressed(const ui::MouseEvent& event) {
  return event.IsOnlyLeftMouseButton();
}

void GroupSummaryRow::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location())) {
    on_activate_.Run();
  }
}

void GroupSummaryRow::OnMouseEntered(const ui::MouseEvent& event) {
  hovered_ = true;
  SchedulePaint();
}

void GroupSummaryRow::OnMouseExited(const ui::MouseEvent& event) {
  hovered_ = false;
  SchedulePaint();
}

bool GroupSummaryRow::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN ||
      event.key_code() == ui::VKEY_SPACE) {
    on_activate_.Run();
    return true;
  }
  return false;
}

void GroupSummaryRow::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTap) {
    on_activate_.Run();
    event->SetHandled();
  }
}

BEGIN_METADATA(GroupSummaryRow)
END_METADATA

}  // namespace atlas::sidebar
