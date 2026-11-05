// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_SIDEBAR_SIDEBAR_SECTIONS_H_
#define CHROME_BROWSER_UI_ATLAS_SIDEBAR_SIDEBAR_SECTIONS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace gfx {
class Canvas;
class Rect;
}  // namespace gfx

namespace ui {
class ColorProvider;
}

namespace atlas::sidebar {

// The labelled sections of the tab list (tokens.md section 3). Sections are
// visual: the model order stays by position, a tab keeps its place and its
// last-active time when it is dragged, and the labels follow the order the rows
// are in.
enum class Section {
  kPinned,
  kToday,
  kYesterday,
  kEarlier,
};

// What the grouping needs to know about one entry of the unpinned list, in
// list order. The engine side fills it from its views; nothing here depends on
// them.
enum class RowKind {
  // A tab on its own: takes part in the sections.
  kTab,
  // A named group: has its own header and ends the run of the tab before it.
  kGroup,
  // Anything else (a split view): carries no label and does not change the run.
  kOther,
};

struct SectionRow {
  RowKind kind = RowKind::kTab;
  // The tab in front is always today's, whatever its recorded time says.
  bool active = false;
  base::Time last_active;
};

// The section of a tab. The boundaries are local midnights.
Section SectionOf(bool active, base::Time last_active, base::Time now);

// For each row, the section whose header is drawn above it, or nullopt. The
// first tab gets a header, and so does a tab whose section differs from the
// previous tab's or that follows a group.
std::vector<std::optional<Section>> SectionHeaders(
    const std::vector<SectionRow>& rows,
    base::Time now);

// Geometry, in DIPs. A header band is the label line plus, in the unpinned
// list, a gap above it so sections do not run into each other.
inline constexpr int kHeaderHeight = 24;
inline constexpr int kHeaderGap = 8;
// The label starts this far to the right of the row's left edge: the row's
// content inset, so the label lines up with the icons below it.
inline constexpr int kHeaderTextInset = 8;

// The height a header takes in the list: zero when there is none, the label
// line alone for the first band of the sidebar (`with_gap` false), the label
// line and the gap above it for the others.
int HeaderBandHeight(bool has_header, bool with_gap);

// The localized label, as drawn (small caps: upper case).
std::u16string SectionLabel(Section section);

// Paints one header: `band` is the whole band including the gap above it; the
// label sits at its bottom, 11 px, weight 600, in the dim colour. `row_left` is
// the x of the left edge of the rows below (the label is inset from it).
void PaintSectionHeader(gfx::Canvas* canvas,
                        const gfx::Rect& band,
                        int row_left,
                        Section section,
                        const ui::ColorProvider& provider);

}  // namespace atlas::sidebar

#endif  // CHROME_BROWSER_UI_ATLAS_SIDEBAR_SIDEBAR_SECTIONS_H_
