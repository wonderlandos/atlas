// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_SIDEBAR_ROW_STYLE_H_
#define CHROME_BROWSER_UI_ATLAS_SIDEBAR_ROW_STYLE_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"

namespace gfx {
class Canvas;
class Rect;
}  // namespace gfx

namespace ui {
class ColorProvider;
}

namespace atlas::sidebar {

// The look of one tab row of the sidebar (tokens.md section 3). Colours come
// from the live colour provider, which follows the system theme: no colour is
// written here, only how the theme's colours are combined.

// Selection, from the design palette: the accent at 22% on a light theme and
// 30% on a dark one.
inline constexpr float kPillAlphaLight = 0.22f;
inline constexpr float kPillAlphaDark = 0.30f;
// The row of a tab that is part of a multi-selection but is not the one in
// front: half of the pill.
inline constexpr float kSelectedRowShare = 0.5f;

enum class RowState {
  // Not hovered, not selected.
  kIdle,
  // The tab in front.
  kActive,
  // Part of a multi-selection, not in front.
  kSelected,
};

// True when the provider is a dark colour set.
bool IsDarkTheme(const ui::ColorProvider& provider);

// The accent tint of the selection pill over a given theme (alpha is part of
// the colour).
SkColor PillColor(const ui::ColorProvider& provider, bool dark);

// The fill behind a row. `hover` is the hover animation value in 0..1; a
// hovered idle row fades to the control fill, a selected row keeps its pill.
SkColor RowFillColor(const ui::ColorProvider& provider,
                     RowState state,
                     double hover);

// The title colour: the text colour in every state (never the dim colour on the
// pill, tokens.md section 3).
SkColor RowTitleColor(const ui::ColorProvider& provider);

// The icon colour: dim when idle, accent on the selected row.
SkColor RowIconColor(const ui::ColorProvider& provider, RowState state);

// The font of a row title: weight 500 for the tab in front, regular otherwise.
gfx::FontList RowTitleFont(const gfx::FontList& current, bool active);

// The colour of section headers and of a collapsed group's summary.
SkColor DimTextColor(const ui::ColorProvider& provider);

// The hairline between the sidebar and the page and the rule above the footer.
SkColor LineColor(const ui::ColorProvider& provider);

// Paints the 1 px line along the edge of the sidebar that faces the page: the
// right edge of `bounds`, or the left one when `on_right` is false.
void PaintSidebarEdge(gfx::Canvas* canvas,
                      const gfx::Rect& bounds,
                      bool on_right,
                      const ui::ColorProvider& provider);

}  // namespace atlas::sidebar

#endif  // CHROME_BROWSER_UI_ATLAS_SIDEBAR_ROW_STYLE_H_
