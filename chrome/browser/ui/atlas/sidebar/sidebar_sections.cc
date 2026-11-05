// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/sidebar/sidebar_sections.h"

#include "base/i18n/case_conversion.h"
#include "chrome/browser/ui/atlas/sidebar/row_style.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"

namespace atlas::sidebar {

namespace {

constexpr int kHeaderFontSize = 11;

int LabelId(Section section) {
  switch (section) {
    case Section::kPinned:
      return IDS_ATLAS_SIDEBAR_SECTION_PINNED;
    case Section::kToday:
      return IDS_ATLAS_SIDEBAR_SECTION_TODAY;
    case Section::kYesterday:
      return IDS_ATLAS_SIDEBAR_SECTION_YESTERDAY;
    case Section::kEarlier:
      return IDS_ATLAS_SIDEBAR_SECTION_EARLIER;
  }
  return IDS_ATLAS_SIDEBAR_SECTION_EARLIER;
}

}  // namespace

Section SectionOf(bool active, base::Time last_active, base::Time now) {
  if (active || last_active.is_null()) {
    return Section::kToday;
  }
  const base::Time today = now.LocalMidnight();
  if (last_active >= today) {
    return Section::kToday;
  }
  // Half a day back from midnight is always inside the day before, whatever
  // the length of that day is (daylight saving).
  const base::Time yesterday = (today - base::Hours(12)).LocalMidnight();
  if (last_active >= yesterday) {
    return Section::kYesterday;
  }
  return Section::kEarlier;
}

std::vector<std::optional<Section>> SectionHeaders(
    const std::vector<SectionRow>& rows,
    base::Time now) {
  std::vector<std::optional<Section>> headers(rows.size());
  std::optional<Section> current;
  for (size_t i = 0; i < rows.size(); ++i) {
    const SectionRow& row = rows[i];
    switch (row.kind) {
      case RowKind::kGroup:
        current.reset();
        break;
      case RowKind::kOther:
        break;
      case RowKind::kTab: {
        const Section section = SectionOf(row.active, row.last_active, now);
        if (!current || *current != section) {
          headers[i] = section;
          current = section;
        }
        break;
      }
    }
  }
  return headers;
}

int HeaderBandHeight(bool has_header, bool with_gap) {
  if (!has_header) {
    return 0;
  }
  return kHeaderHeight + (with_gap ? kHeaderGap : 0);
}

std::u16string SectionLabel(Section section) {
  return base::i18n::ToUpper(l10n_util::GetStringUTF16(LabelId(section)));
}

void PaintSectionHeader(gfx::Canvas* canvas,
                        const gfx::Rect& band,
                        int row_left,
                        Section section,
                        const ui::ColorProvider& provider) {
  const gfx::FontList base_font;
  const gfx::FontList font =
      base_font.DeriveWithSizeDelta(kHeaderFontSize - base_font.GetFontSize())
          .DeriveWithWeight(gfx::Font::Weight::SEMIBOLD);
  const gfx::Rect label(
      row_left + kHeaderTextInset, band.bottom() - kHeaderHeight,
      band.width() - row_left - kHeaderTextInset, kHeaderHeight);
  canvas->DrawStringRectWithFlags(
      SectionLabel(section), font, DimTextColor(provider), label,
      gfx::Canvas::TEXT_ALIGN_LEFT | gfx::Canvas::NO_SUBPIXEL_RENDERING);
}

}  // namespace atlas::sidebar
